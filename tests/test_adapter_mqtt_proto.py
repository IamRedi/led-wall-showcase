"""MQTT 3.1.1 codec tests for the adapter's minimal client (C-L005-1).

Expected byte sequences are derived independently from the MQTT 3.1.1
specification (packet layouts in §2.2, §3.1 CONNECT, §3.8 SUBSCRIBE,
§3.3 PUBLISH), not from the implementation.
"""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from ha_adapter.mqtt_proto import (  # noqa: E402
    MqttProtocolError,
    PacketReader,
    Puback,
    Publish,
    decode_packet,
    decode_remaining_length,
    encode_connect,
    encode_disconnect,
    encode_pingreq,
    encode_puback,
    encode_publish,
    encode_remaining_length,
    encode_subscribe,
)


class RemainingLengthVarintTest(unittest.TestCase):
    """MQTT 3.1.1 §2.2.3: 1..4 bytes, 7 bits each, LSB first, continuation bit."""

    def test_boundaries(self):
        cases = {
            0: b"\x00",
            1: b"\x01",
            127: b"\x7f",
            128: b"\x80\x01",
            129: b"\x81\x01",
            16383: b"\xff\x7f",
            16384: b"\x80\x80\x01",
            2097151: b"\xff\xff\x7f",
            2097152: b"\x80\x80\x80\x01",
            268435455: b"\xff\xff\xff\x7f",  # protocol maximum
        }
        for value, expected in cases.items():
            with self.subTest(value=value):
                self.assertEqual(encode_remaining_length(value), expected)

    def test_above_protocol_maximum_rejected(self):
        with self.assertRaises(MqttProtocolError):
            encode_remaining_length(268435456)
        with self.assertRaises(MqttProtocolError):
            encode_remaining_length(-1)

    def test_round_trip(self):
        for value in (0, 1, 127, 128, 16383, 16384, 2097151, 2097152, 268435455):
            encoded = encode_remaining_length(value)
            decoded, pos = decode_remaining_length(encoded + b"\xff", 0)
            self.assertEqual((decoded, pos), (value, len(encoded)))


class ConnectEncodingTest(unittest.TestCase):
    def test_minimal_clean_session(self):
        # §3.1: variable header = "MQTT"(0x0004...) + level 4 + flags + keepalive.
        packet = encode_connect("lw", 30, clean_session=True)
        self.assertEqual(
            packet,
            b"\x10\x0e"
            b"\x00\x04MQTT"  # protocol name
            b"\x04"          # protocol level (3.1.1)
            b"\x02"          # clean session flag
            b"\x00\x1e"      # keepalive 30 s
            b"\x00\x02lw",   # client id
        )

    def test_persistent_session_flag_clear(self):
        packet = encode_connect("lw", 30, clean_session=False)
        self.assertEqual(packet[10], 0x00)

    def test_username_password_flags(self):
        packet = encode_connect("lw", 30, clean_session=True, username="u", password="p")
        self.assertEqual(
            packet,
            b"\x10\x14"
            b"\x00\x04MQTT\x04"
            b"\xc2"  # username (0x80) | password (0x40) | clean session (0x02)
            b"\x00\x1e"
            b"\x00\x02lw\x00\x01u\x00\x01p",
        )

    def test_last_will_retained_qos0(self):
        packet = encode_connect(
            "lw", 30, clean_session=True,
            will_topic="avail", will_payload="offline", will_retain=True, will_qos=0,
        )
        self.assertEqual(
            packet,
            b"\x10\x1e"
            b"\x00\x04MQTT\x04"
            b"\x26"  # will flag (0x04) | will retain (0x20) | clean session (0x02)
            b"\x00\x1e"
            b"\x00\x02lw"
            b"\x00\x05avail\x00\x07offline",
        )

    def test_empty_client_id_rejected(self):
        with self.assertRaises(MqttProtocolError):
            encode_connect("", 30, clean_session=True)


class SubscribePublishEncodingTest(unittest.TestCase):
    def test_subscribe_qos1(self):
        # §3.8: fixed header flags must be 0010 (0x82); body = pid + (topic, qos).
        packet = encode_subscribe(1, [("a/b", 1)])
        self.assertEqual(packet, b"\x82\x08\x00\x01\x00\x03a/b\x01")

    def test_subscribe_empty_rejected(self):
        with self.assertRaises(MqttProtocolError):
            encode_subscribe(1, [])

    def test_publish_qos1_retain(self):
        packet = encode_publish("t", b"hi", qos=1, retain=True, packet_id=7)
        # topic (2 len + 1) + packet id (2) + payload (2) = 7 body bytes
        self.assertEqual(packet, b"\x33\x07\x00\x01t\x00\x07hi")

    def test_publish_qos0_plain(self):
        packet = encode_publish("t", b"x", qos=0, retain=False, packet_id=None)
        self.assertEqual(packet, b"\x30\x04\x00\x01tx")

    def test_publish_qos0_retain(self):
        packet = encode_publish("t", b"x", qos=0, retain=True, packet_id=None)
        self.assertEqual(packet, b"\x31\x04\x00\x01tx")

    def test_publish_qos1_requires_packet_id(self):
        with self.assertRaises(MqttProtocolError):
            encode_publish("t", b"x", qos=1, retain=False, packet_id=None)

    def test_puback_pingreq_disconnect(self):
        self.assertEqual(encode_puback(5), b"\x40\x02\x00\x05")
        self.assertEqual(encode_pingreq(), b"\xc0\x00")
        self.assertEqual(encode_disconnect(), b"\xe0\x00")


class DecodeTest(unittest.TestCase):
    def test_connack_success(self):
        packet = decode_packet(0x20, b"\x00\x00")
        self.assertEqual((packet.session_present, packet.return_code), (False, 0))

    def test_connack_session_present_and_refused(self):
        packet = decode_packet(0x20, b"\x01\x05")
        self.assertEqual((packet.session_present, packet.return_code), (True, 5))

    def test_connack_wrong_length_rejected(self):
        with self.assertRaises(MqttProtocolError):
            decode_packet(0x20, b"\x00")

    def test_suback(self):
        packet = decode_packet(0x90, b"\x00\x03\x01\x00")
        self.assertEqual(packet.packet_id, 3)
        self.assertEqual(packet.return_codes, (1, 0))

    def test_suback_invalid_code_rejected(self):
        with self.assertRaises(MqttProtocolError):
            decode_packet(0x90, b"\x00\x03\x05")

    def test_publish_qos1_from_broker(self):
        packet = decode_packet(0x32, b"\x00\x03cmd\x00\x09" + b'{"x":1}')
        self.assertIsInstance(packet, Publish)
        self.assertEqual(packet.topic, "cmd")
        self.assertEqual(packet.payload, b'{"x":1}')
        self.assertEqual(packet.packet_id, 9)
        self.assertFalse(packet.retain)
        self.assertFalse(packet.dup)

    def test_publish_qos3_invalid(self):
        with self.assertRaises(MqttProtocolError):
            decode_packet(0x36, b"\x00\x01t")

    def test_publish_wildcard_topic_rejected(self):
        with self.assertRaises(MqttProtocolError):
            decode_packet(0x30, b"\x00\x03a/+x")

    def test_publish_qos1_without_pid_rejected(self):
        with self.assertRaises(MqttProtocolError):
            decode_packet(0x32, b"\x00\x01t")

    def test_puback(self):
        self.assertEqual(decode_packet(0x40, b"\x00\x07"), Puback(packet_id=7))

    def test_pingresp(self):
        decode_packet(0xD0, b"")

    def test_unexpected_type_rejected(self):
        with self.assertRaises(MqttProtocolError):
            decode_packet(0xE0, b"")  # brokers must not send DISCONNECT


class PacketReaderTest(unittest.TestCase):
    def _qos0_publish(self, topic="t", payload=b"abc"):
        return encode_publish(topic, payload, qos=0, retain=False, packet_id=None)

    def test_single_packet(self):
        reader = PacketReader()
        packets = reader.feed(self._qos0_publish())
        self.assertEqual(len(packets), 1)
        self.assertEqual(packets[0].topic, "t")
        self.assertEqual(packets[0].payload, b"abc")

    def test_byte_by_byte_fragmentation(self):
        reader = PacketReader()
        data = self._qos0_publish() + self._qos0_publish(topic="u", payload=b"z")
        first_packet_len = len(self._qos0_publish())
        seen = []
        for index in range(len(data)):
            seen.extend(reader.feed(data[index : index + 1]))
            if index < first_packet_len - 1:
                self.assertEqual(seen, [])  # nothing parses before a full packet
            elif index < len(data) - 1:
                self.assertEqual(len(seen), 1)  # only the first packet so far
        self.assertEqual(len(seen), 2)
        self.assertEqual([p.topic for p in seen], ["t", "u"])

    def test_two_packets_in_one_feed(self):
        reader = PacketReader()
        packets = reader.feed(self._qos0_publish() + self._qos0_publish(topic="b", payload=b""))
        self.assertEqual([p.topic for p in packets], ["t", "b"])

    def test_oversized_packet_declared_in_header_is_fatal(self):
        reader = PacketReader(max_packet_size=1024)
        with self.assertRaises(MqttProtocolError):
            reader.feed(b"\x30" + encode_remaining_length(2048))

    def test_incomplete_varint_header_waits_for_more_bytes(self):
        reader = PacketReader()
        self.assertEqual(reader.feed(b"\x30"), [])   # fixed header only
        self.assertEqual(reader.feed(b"\x80"), [])   # continuation bit pending
        self.assertEqual(reader.feed(b"\x80\x80"), [])  # still only 2 of 4 varint bytes

    def test_malformed_body_within_declared_length_is_fatal(self):
        reader = PacketReader()
        # PUBLISH declaring a 1-byte body: cannot hold a topic length header.
        with self.assertRaises(MqttProtocolError):
            reader.feed(b"\x30\x01\x00")


if __name__ == "__main__":
    unittest.main()
