"""One outstanding request; no automatic retry, enable, recovery, or hardware IO."""
from dataclasses import dataclass
from enum import IntEnum
import math
import struct

from .transport import Transport
from .wire import Frame, Message, Parser, ProtocolError, VERSION, MAX_WIRE, encode


class Error(IntEnum):
    OK = 0
    BAD_PAYLOAD = 1
    UNSUPPORTED_VERSION = 2
    WRONG_DIRECTION = 3
    NO_SESSION = 4
    BAD_SESSION = 5
    BAD_SEQUENCE = 6
    STALE_TOKEN = 7
    NOT_CONFIGURED = 8
    CONFIG_MISMATCH = 9
    DISABLED = 10
    PHYSICAL_LOCKED = 11
    INVALID_JOINT = 12
    INVALID_VALUE = 13
    OUT_OF_RANGE = 14
    UNCALIBRATED = 15
    INVALID_CONFIGURATION = 16
    DUPLICATE_JOINT = 17
    STOPPED = 18
    ACTUATOR_FAILURE = 19


@dataclass(frozen=True)
class Outcome:
    accepted: bool
    state: int
    fault: int
    physical_permission: bool = False

    @property
    def simulation_enabled(self):
        return self.state == 1


@dataclass(frozen=True)
class JointStatus:
    joint: int
    controller: int | None
    channel: int | None
    calibrated: bool
    commanded_rad: float | None


@dataclass(frozen=True)
class Status:
    state: int
    fault: int
    physical_permission: bool
    configuration_valid: bool
    session_active: bool
    configured_controllers: int
    configuration_tag: int
    malformed_frames: int
    joints: tuple[JointStatus, ...]


class RemoteError(ProtocolError):
    def __init__(self, code: Error, outcome: Outcome):
        self.code, self.outcome = code, outcome
        super().__init__(f"remote rejected request: {code.name}")


class Client:
    def __init__(self, transport: Transport):
        self.transport = transport
        self.session = self.token = self.sequence = 0
        self.configuration_tag: int | None = None
        self.heartbeat_timeout_us: int | None = None
        self.command_timeout_us: int | None = None

    def _request(self, message: Message, payload=b"") -> Frame:
        if self.sequence == 0xFFFFFFFF:
            raise ProtocolError("request IDs exhausted; create a new client/handshake")
        self.sequence += 1
        sent = Frame(message, self.sequence, self.session, self.token, payload)
        self.transport.write(encode(sent))
        parser = Parser()
        # Reference transports must make the reply available before read returns
        # empty. Real async scheduling/deadlines belong in a future adapter.
        received = None
        for _ in range(MAX_WIRE):
            chunk = self.transport.read(1)
            if not chunk:
                break
            if len(chunk) != 1:
                raise ProtocolError("transport exceeded requested read size")
            frame = parser.push(chunk[0])
            if frame is not None:
                received = frame
                break
        if received is None:
            raise ProtocolError("no complete valid response; request is not retried")
        if received.request != sent.request:
            raise ProtocolError("response request ID mismatch")
        allowed = {
            Message.HELLO: Message.HELLO_ACK,
            Message.HEARTBEAT: Message.HEARTBEAT_REPLY,
            Message.GET_STATUS: Message.STATUS,
        }.get(message, Message.ACK)
        if received.message not in (allowed, Message.NACK):
            raise ProtocolError("unexpected response type")
        if message not in (Message.HELLO, Message.DISABLE, Message.EMERGENCY_STOP, Message.GET_STATUS):
            if received.message != Message.NACK and received.session != sent.session:
                raise ProtocolError("response session mismatch")
        if received.message in (Message.ACK, Message.NACK, Message.HEARTBEAT_REPLY):
            if len(received.payload) != 5:
                raise ProtocolError("invalid outcome length")
            echo, code, state, fault, physical = received.payload
            if echo != message or physical != 0 or state > 2 or fault > 6:
                raise ProtocolError("invalid outcome/physical permission")
            try:
                error = Error(code)
            except ValueError as exc:
                raise ProtocolError("unknown remote error") from exc
            if (received.message == Message.NACK) != (error != Error.OK):
                raise ProtocolError("inconsistent ACK/NACK")
            if error != Error.OK:
                if received.session == 0:
                    self.session = self.token = 0
                raise RemoteError(error, Outcome(False, state, fault))
        if message == Message.HEARTBEAT:
            if received.token == 0:
                raise ProtocolError("missing freshness token")
            self.token = received.token
        return received

    def hello(self, minimum=VERSION, maximum=VERSION):
        self.session = self.token = 0
        self.configuration_tag = None
        response = self._request(Message.HELLO, bytes([minimum, maximum]))
        if len(response.payload) != 22:
            raise ProtocolError("invalid HELLO_ACK length")
        selected, tag, heartbeat, command, physical = struct.unpack("<BIQQB", response.payload)
        if selected != VERSION or physical != 0 or not all((tag, heartbeat, command, response.session, response.token)):
            raise ProtocolError("invalid HELLO_ACK capabilities")
        self.session, self.token = response.session, response.token
        self.configuration_tag = tag
        self.heartbeat_timeout_us, self.command_timeout_us = heartbeat, command
        return response

    def _command(self, message, payload=b""):
        response = self._request(message, payload)
        return Outcome(True, response.payload[2], response.payload[3])

    def heartbeat(self):
        return self._command(Message.HEARTBEAT)

    def enable(self):
        if self.configuration_tag is None:
            raise ProtocolError("handshake required")
        return self._command(Message.ENABLE_REQUEST, struct.pack("<I", self.configuration_tag))

    def disable(self):
        try:
            return self._command(Message.DISABLE)
        finally:
            self.session = self.token = 0

    def emergency_stop(self):
        try:
            return self._command(Message.EMERGENCY_STOP)
        finally:
            self.session = self.token = 0

    @staticmethod
    def _target(joint: int, radians: float) -> bytes:
        if not isinstance(joint, int) or not 0 <= joint < 18 or not math.isfinite(radians):
            raise ProtocolError("invalid joint or non-finite target")
        return struct.pack("<Bd", joint, radians)

    def set_joint(self, joint: int, radians: float):
        return self._command(Message.SET_JOINT_TARGET, self._target(joint, radians))

    def set_multi(self, targets: list[tuple[int, float]]):
        if not 1 <= len(targets) <= 18 or len({j for j, _ in targets}) != len(targets):
            raise ProtocolError("invalid target count or duplicate joint")
        payload = bytes([len(targets)]) + b"".join(self._target(j, r) for j, r in targets)
        return self._command(Message.SET_MULTI_JOINT_TARGET, payload)

    def status(self) -> Status:
        response = self._request(Message.GET_STATUS)
        p = response.payload
        if len(p) != 249:
            raise ProtocolError("invalid STATUS length")
        state, fault, physical, valid, active, controllers, count, tag, malformed = struct.unpack_from("<7BII", p)
        if physical != 0 or state > 2 or fault > 6 or valid > 1 or active > 1 or controllers > 3 or count != 18:
            raise ProtocolError("invalid STATUS fields")
        joints = []
        for i in range(18):
            joint, controller, channel, calibrated, has_target, angle = struct.unpack_from("<5Bd", p, 15 + 13 * i)
            if (joint != i or controller not in (0, 1, 255) or channel not in (*range(16), 255)
                    or calibrated > 1 or has_target > 1 or not math.isfinite(angle)):
                raise ProtocolError("invalid joint telemetry")
            joints.append(JointStatus(joint, None if controller == 255 else controller,
                                      None if channel == 255 else channel,
                                      bool(calibrated), angle if has_target else None))
        return Status(state, fault, False, bool(valid), bool(active), controllers, tag, malformed, tuple(joints))
