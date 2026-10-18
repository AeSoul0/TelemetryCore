"""Unit tests for the ground station protocol parser: python -m unittest discover -s tools/ground_station"""

import unittest

from telemetry_protocol import (
    Boot,
    FrameError,
    LinkStats,
    LogRecord,
    PayloadError,
    Telemetry,
    Unknown,
    checksum,
    decode,
    describe_flags,
    encode,
    parse_line,
)


class FrameTests(unittest.TestCase):
    def test_checksum_matches_firmware(self):
        # Same vectors as tests/test_frame.cpp
        self.assertEqual(checksum("A"), 0x41)
        self.assertEqual(checksum("jp"), 0x1A)
        self.assertEqual(encode("A"), "$A*41\r\n")

    def test_decode_round_trip(self):
        self.assertEqual(decode(encode("TLM,1,2,3")), "TLM,1,2,3")
        self.assertEqual(decode(b"$jp*1a\r\n"), "jp")

    def test_decode_rejects_corruption(self):
        for line in ("$A*42", "A*41", "$A41", "$A*4G", "", "$B*41"):
            with self.subTest(line=line):
                with self.assertRaises(FrameError):
                    decode(line)


class ParseTests(unittest.TestCase):
    def test_telemetry(self):
        message = parse_line(encode("TLM,42,4200,23.6,1013.3,87,OK,00"))
        self.assertEqual(message, Telemetry(42, 4200, 23.6, 1013.3, 87, "OK", 0))
        self.assertEqual(message.faults, [])

    def test_telemetry_with_faults(self):
        message = parse_line(encode("TLM,1,100,-0.5,1000.0,3,CRITICAL,09"))
        self.assertEqual(message.temperature_c, -0.5)
        self.assertEqual(message.faults, ["SENSOR_ERROR", "BATTERY_LOW"])

    def test_malformed_telemetry(self):
        with self.assertRaises(PayloadError):
            parse_line(encode("TLM,1,2,3"))
        with self.assertRaises(PayloadError):
            parse_line(encode("TLM,x,100,1.0,2.0,3,OK,00"))

    def test_log_message_may_contain_commas(self):
        message = parse_line(encode("LOG,WARN,SensorTask,jitter 3 ms, q1 0/10"))
        self.assertEqual(message, LogRecord("WARN", "SensorTask", "jitter 3 ms, q1 0/10"))

    def test_boot(self):
        message = parse_line(encode("BOOT,fw=0.4.0,board=nucleo-f446re"))
        self.assertIsInstance(message, Boot)
        self.assertEqual(message.firmware, "0.4.0")
        self.assertEqual(message.board, "nucleo-f446re")

    def test_unknown_kind_is_kept(self):
        self.assertEqual(parse_line(encode("ACK,HELLO")), Unknown("ACK", "ACK,HELLO"))

    def test_unknown_flag_bits(self):
        self.assertEqual(describe_flags(0x81), ["SENSOR_ERROR", "0x80"])


class LinkStatsTests(unittest.TestCase):
    @staticmethod
    def tlm(seq):
        return Telemetry(seq, seq * 100, 20.0, 1013.0, 90, "OK", 0)

    def test_detects_lost_packets(self):
        stats = LinkStats()
        for seq in (0, 1, 2, 5, 6):
            stats.record(self.tlm(seq))
        self.assertEqual(stats.lost, 2)
        self.assertFalse(stats.healthy)

    def test_boot_resets_sequence(self):
        stats = LinkStats()
        stats.record(self.tlm(0))
        stats.record(self.tlm(1))
        stats.record(Boot({"fw": "1"}))
        stats.record(self.tlm(0))
        self.assertEqual(stats.lost, 0)
        self.assertTrue(stats.healthy)

    def test_counts_errors(self):
        stats = LinkStats()
        stats.record_error(FrameError("bad"))
        stats.record_error(PayloadError("bad"))
        self.assertEqual((stats.checksum_errors, stats.malformed), (1, 1))
        self.assertFalse(stats.healthy)


class DashboardTests(unittest.TestCase):
    def test_render_shows_latest_values(self):
        from ground_station import Dashboard

        stats = LinkStats()
        dashboard = Dashboard("test", color=False)
        for message in (
            Boot({"fw": "0.4.0", "board": "host-sim"}),
            Telemetry(7, 61_500, 23.6, 1013.3, 42, "WARNING", 0x01),
            LogRecord("WARN", "SensorTask", "sensor not responding after 3 attempts"),
        ):
            stats.record(message)
            dashboard.update(message)
        text = dashboard.render(stats)
        for expected in ("fw 0.4.0", "board host-sim", "WARNING", "23.6 °C", "1013.3 hPa", "42 %",
                         "SENSOR_ERROR", "00:01:01.5", "sensor not responding"):
            self.assertIn(expected, text)


if __name__ == "__main__":
    unittest.main()
