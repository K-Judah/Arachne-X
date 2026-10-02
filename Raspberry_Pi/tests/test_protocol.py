"""Stdlib tests, including exchanges with the actual C++ parser/dispatcher."""
import argparse
import binascii
from pathlib import Path
import random
import struct
import subprocess
import sys
import types
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Raspberry_Pi"))
from arachne_protocol import Client, Frame, Message, ProtocolError, RemoteError, decode, encode
from arachne_protocol.client import Error
from arachne_protocol.transport import MemoryTransport
from arachne_protocol.wire import Parser, HEADER, MAX_PAYLOAD, MAX_WIRE, crc16, cobs_decode, cobs_encode

ARGS = None


def altered(frame, offset, replacement):
    raw = bytearray(cobs_decode(encode(frame)[:-1]))
    raw[offset:offset + len(replacement)] = replacement
    raw[-2:] = struct.pack("<H", binascii.crc_hqx(raw[:-2], 0xFFFF))
    return cobs_encode(raw) + b"\0"


class CodecTests(unittest.TestCase):
    def test_crc_reference(self):
        self.assertEqual(crc16(b"123456789"), 0x29B1)

    def test_roundtrip_fields(self):
        frame = Frame(Message.SET_JOINT_TARGET, 0xFFFFFFFF, 0x123456789ABCDEF0,
                      0x98765432, struct.pack("<Bd", 17, 0.2))
        self.assertEqual(decode(encode(frame)), frame)

    def test_all_payload_lengths_zero_and_nonzero(self):
        for length in range(MAX_PAYLOAD + 1):
            for byte in (0, 1, 255):
                frame = Frame(Message.STATUS, 1, payload=bytes([byte]) * length)
                self.assertEqual(decode(encode(frame)), frame)
                self.assertLessEqual(len(encode(frame)), MAX_WIRE)

    def test_truncated_frames(self):
        wire = encode(Frame(Message.HELLO, 1, payload=b"\1\1"))
        for length in range(len(wire)):
            with self.assertRaises(ProtocolError):
                decode(wire[:length])

    def test_corruption(self):
        wire = bytearray(encode(Frame(Message.GET_STATUS, 1)))
        wire[-2] ^= 1
        with self.assertRaises(ProtocolError):
            decode(wire)

    def test_unsupported_version(self):
        with self.assertRaisesRegex(ProtocolError, "version"):
            decode(altered(Frame(Message.GET_STATUS, 1), 0, b"\2"))

    def test_unknown_message(self):
        with self.assertRaisesRegex(ProtocolError, "unknown message"):
            decode(altered(Frame(Message.GET_STATUS, 1), 1, b"\x63"))

    def test_bad_length(self):
        with self.assertRaisesRegex(ProtocolError, "length"):
            decode(altered(Frame(Message.GET_STATUS, 1), 18, b"\1\0"))

    def test_oversized_encode_and_header_range(self):
        for frame in (Frame(Message.STATUS, 1, payload=b"x" * 257),
                      Frame(Message.GET_STATUS, -1), Frame(Message.GET_STATUS, 1, session=2**64)):
            with self.assertRaises(ProtocolError):
                encode(frame)

    def test_parser_resynchronizes_and_detects_partial_stream(self):
        parser = Parser()
        for byte in b"\1" * (MAX_WIRE * 3) + b"\0":
            self.assertIsNone(parser.push(byte))
        self.assertEqual(parser.errors, 1)
        frame = Frame(Message.GET_STATUS, 42)
        results = [parser.push(byte) for byte in encode(frame)]
        self.assertEqual([r for r in results if r], [frame])
        parser.push(3)
        with self.assertRaisesRegex(ProtocolError, "truncated"):
            parser.finish()

    def test_random_bytes_are_bounded(self):
        randomizer = random.Random(0x41525831)
        parser = Parser()
        for _ in range(10000):
            data = randomizer.randbytes(randomizer.randrange(1, MAX_WIRE)) + b"\0"
            for byte in data:
                self.assertIsNone(parser.push(byte))
                self.assertLessEqual(len(parser.buffer), MAX_WIRE)

    def test_shared_golden_vectors_and_independent_stdlib_crc(self):
        count = 0
        for line in (ROOT / "protocol/golden_vectors.txt").read_text().splitlines():
            if not line or line.startswith("#"):
                continue
            _, message, request, session, token, payload, wire = line.split()
            frame = Frame(Message(int(message)), int(request), int(session), int(token),
                          b"" if payload == "-" else bytes.fromhex(payload))
            self.assertEqual(encode(frame).hex(), wire)
            self.assertEqual(decode(bytes.fromhex(wire)), frame)
            raw = cobs_decode(bytes.fromhex(wire)[:-1])
            self.assertEqual(binascii.crc_hqx(raw[:-2], 0xFFFF), struct.unpack("<H", raw[-2:])[0])
            count += 1
        self.assertGreaterEqual(count, 3)

    def test_memory_transport_bounded_fifo_and_close(self):
        left, right = MemoryTransport.pair()
        left.write(b"abc")
        self.assertEqual(right.read(2), b"ab")
        self.assertEqual(right.read(2), b"c")
        with self.assertRaises(ProtocolError):
            left.write(b"x" * (left.CAPACITY + 1))
        self.assertEqual(right.read(2), b"")
        right.closed = True
        with self.assertRaises(ProtocolError):
            left.write(b"x")

    def test_client_validates_values_without_writing(self):
        left, right = MemoryTransport.pair()
        client = Client(left)
        for joint, angle in ((18, 0.2), (0, float("nan")), (0, float("inf"))):
            with self.assertRaises(ProtocolError):
                client.set_joint(joint, angle)
        for targets in ([], [(0, 0.2)] * 2, [(i, 0.2) for i in range(19)]):
            with self.assertRaises(ProtocolError):
                client.set_multi(targets)
        with self.assertRaises(ProtocolError):
            client.enable()
        self.assertEqual(right.read(100), b"")

    def test_client_rejects_wrong_response_id_type_and_permission(self):
        class Responder:
            def __init__(self, response): self.response, self.buffer = response, bytearray()
            def write(self, data): self.buffer.extend(encode(self.response(decode(data))))
            def read(self, maximum):
                data = bytes(self.buffer[:maximum]); del self.buffer[:maximum]; return data
        responses = (
            lambda r: Frame(Message.ACK, r.request + 1, payload=b"\5\0\0\0\0"),
            lambda r: Frame(Message.HELLO_ACK, r.request),
            lambda r: Frame(Message.ACK, r.request, payload=b"\5\0\0\0\1"),
        )
        for response in responses:
            with self.assertRaises(ProtocolError):
                Client(Responder(response)).disable()

    def test_upload_guard_remains_active(self):
        source = (ROOT / "Firmware/scripts/deny_upload.py").read_text()
        module = types.ModuleType("SCons.Script")
        previous = {name: sys.modules.get(name) for name in ("SCons", "SCons.Script")}
        sys.modules["SCons"] = types.ModuleType("SCons")
        sys.modules["SCons.Script"] = module
        try:
            for targets, rejected in (([], False), (["buildprog"], False),
                                      (["upload"], True), (["uploadfs"], True), (["program"], True)):
                module.COMMAND_LINE_TARGETS = targets
                if rejected:
                    with self.assertRaisesRegex(RuntimeError, "intentionally disabled"):
                        exec(compile(source, "deny_upload.py", "exec"), {"Import": lambda _: None})
                else:
                    exec(compile(source, "deny_upload.py", "exec"), {"Import": lambda _: None})
        finally:
            for name, value in previous.items():
                if value is None: del sys.modules[name]
                else: sys.modules[name] = value


class BridgeTransport:
    """Test-only process adapter. Actual protocol bytes go through C++ memory IO."""
    def __init__(self, executable, *args):
        self.process = subprocess.Popen([executable, *args], stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.buffer = bytearray()

    def line(self, text):
        self.process.stdin.write(text + "\n")
        self.process.stdin.flush()
        line = self.process.stdout.readline()
        if not line:
            raise RuntimeError("C++ endpoint exited: " + self.process.stderr.read())
        return line.strip()

    def write(self, data):
        response = self.line("DATA " + data.hex())
        if response != "-": self.buffer.extend(bytes.fromhex(response))

    def read(self, maximum):
        result = bytes(self.buffer[:maximum]); del self.buffer[:maximum]; return result

    def close(self):
        self.process.stdin.close()
        self.process.wait(timeout=5)
        self.process.stdout.close()
        self.process.stderr.close()
        if self.process.returncode != 0: raise RuntimeError("bridge failed")


class EndToEndTests(unittest.TestCase):
    def setUp(self):
        self.transport = BridgeTransport(ARGS.bridge)
        self.addCleanup(self.transport.close)
        self.client = Client(self.transport)

    def count(self): return int(self.transport.line("COUNT"))

    def raw_request(self, frame):
        self.transport.write(encode(frame))
        return decode(self.transport.read(MAX_WIRE))

    def test_full_client_exchange_and_three_distinct_permissions(self):
        self.assertEqual(self.client.status().state, 0)
        self.client.hello()
        self.assertTrue(self.client.heartbeat().accepted)
        enabled = self.client.enable()
        self.assertTrue(enabled.accepted and enabled.simulation_enabled)
        self.assertFalse(enabled.physical_permission)
        self.assertEqual(self.count(), 0)
        self.client.set_joint(0, 0.2)
        self.client.set_multi([(1, 0.3), (17, 0.4)])
        status = self.client.status()
        self.assertEqual(self.count(), 3)
        self.assertEqual(status.joints[17].commanded_rad, 0.4)
        self.assertIsNone(status.joints[5].commanded_rad)
        self.assertFalse(status.physical_permission)
        self.assertEqual(self.client.disable().state, 0)

    def test_no_implicit_enable(self):
        self.client.hello()
        with self.assertRaises(RemoteError) as caught: self.client.set_joint(0, 0.2)
        self.assertEqual(caught.exception.code, Error.DISABLED)
        self.assertEqual(self.count(), 0)

    def test_version_negotiation(self):
        with self.assertRaises(RemoteError) as caught: self.client.hello(2, 3)
        self.assertEqual(caught.exception.code, Error.UNSUPPORTED_VERSION)
        self.client.hello(1, 3)
        self.assertGreater(self.client.session, 0)

    def test_endpoint_rejects_raw_invalid_joint_nan_and_payload(self):
        self.client.hello(); self.client.enable()
        for sequence, payload, error in ((3, struct.pack("<Bd", 255, 0.2), Error.INVALID_JOINT),
                                         (4, struct.pack("<Bd", 0, float("nan")), Error.INVALID_VALUE),
                                         (5, b"", Error.BAD_PAYLOAD)):
            response = self.raw_request(Frame(Message.SET_JOINT_TARGET, sequence,
                                        self.client.session, self.client.token, payload))
            self.assertEqual(response.message, Message.NACK)
            self.assertEqual(response.payload[1], error)
        self.assertEqual(self.count(), 0)

    def test_multi_rejection_is_atomic(self):
        self.client.hello(); self.client.enable()
        with self.assertRaises(RemoteError) as caught: self.client.set_multi([(0, 0.2), (1, 99.0)])
        self.assertEqual(caught.exception.code, Error.OUT_OF_RANGE)
        self.assertEqual(self.count(), 0)
        self.client.set_multi([(0, 0.2), (1, 0.3)])
        self.assertEqual(self.count(), 2)

    def test_uncalibrated_joint(self):
        with_bridge = BridgeTransport(ARGS.bridge, "uncalibrated")
        self.addCleanup(with_bridge.close)
        client = Client(with_bridge); client.hello()
        with self.assertRaises(RemoteError) as caught: client.set_joint(0, 0.2)
        self.assertEqual(caught.exception.code, Error.UNCALIBRATED)
        self.assertEqual(with_bridge.line("COUNT"), "0")

    def test_locked_firmware_reference_client_cannot_enable(self):
        transport = BridgeTransport(ARGS.locked_bridge)
        self.addCleanup(transport.close)
        client = Client(transport); client.hello()
        with self.assertRaises(RemoteError) as caught: client.enable()
        self.assertEqual(caught.exception.code, Error.PHYSICAL_LOCKED)
        self.assertFalse(caught.exception.outcome.physical_permission)
        self.assertEqual(client.status().state, 0)
        self.assertEqual(transport.line("COUNT"), "0")

    def test_estop_persists_through_disable_and_handshake(self):
        self.client.hello(); self.client.enable(); self.client.emergency_stop()
        self.assertEqual(self.client.disable().state, 2)
        for action in (self.client.hello, self.client.heartbeat, lambda: self.client.set_joint(0, 0.2)):
            with self.assertRaises(RemoteError) as caught: action()
            self.assertEqual(caught.exception.code, Error.STOPPED)
        self.assertEqual(self.client.status().fault, 1)
        self.assertEqual(self.count(), 0)

    def test_heartbeat_timeout_requires_fresh_handshake_and_enable(self):
        self.client.hello()
        self.transport.line("TIME 1000")
        with self.assertRaises(RemoteError) as caught: self.client.heartbeat()
        self.assertEqual(caught.exception.code, Error.NO_SESSION)
        self.assertEqual(self.client.status().fault, 2)
        self.client.hello()
        self.assertEqual(self.client.status().state, 0)

    def test_heartbeats_do_not_extend_motion_lease(self):
        self.client.hello(); self.client.enable()
        self.transport.line("TIME 400"); self.client.heartbeat()
        self.transport.line("TIME 500")
        status = self.client.status()
        self.assertEqual((status.state, status.fault), (0, 3))

    def test_corrupt_and_random_frames_never_forward_commands(self):
        self.client.hello(); self.client.enable()
        randomizer = random.Random(4321)
        for _ in range(100):
            self.transport.write(randomizer.randbytes(randomizer.randrange(1, MAX_WIRE)) + b"\0")
            self.assertEqual(self.transport.read(MAX_WIRE), b"")
        self.assertEqual(self.count(), 0)
        self.assertGreater(self.client.status().malformed_frames, 0)

    def test_truncated_frame_timeout_and_disconnect(self):
        self.client.hello(); self.client.enable()
        self.transport.write(b"\3\1")
        self.transport.line("TIME 100")
        self.assertEqual(self.client.status().state, 0)
        self.assertGreater(self.client.status().malformed_frames, 0)
        self.client.hello(); self.client.enable(); self.transport.line("DROP")
        self.assertFalse(self.client.status().session_active)

    def test_duplicate_request_does_not_repeat_command(self):
        self.client.hello(); self.client.enable()
        request = Frame(Message.SET_JOINT_TARGET, 3, self.client.session,
                        self.client.token, struct.pack("<Bd", 0, 0.2))
        self.assertEqual(self.raw_request(request).message, Message.ACK)
        self.assertEqual(self.raw_request(request).payload[1], Error.BAD_SEQUENCE)
        self.assertEqual(self.count(), 1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--bridge", required=True)
    parser.add_argument("--locked-bridge", required=True)
    ARGS, remaining = parser.parse_known_args()
    unittest.main(argv=[sys.argv[0], *remaining], verbosity=2)
