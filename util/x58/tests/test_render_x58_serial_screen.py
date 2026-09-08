import importlib.util
from pathlib import Path
import unittest


SPEC = importlib.util.spec_from_file_location(
    "render_x58_serial_screen",
    Path(__file__).resolve().parents[1] / "scripts/render_x58_serial_screen.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)
render = MODULE.render


class ScreenTests(unittest.TestCase):
    def test_redraw_does_not_duplicate_prefix(self):
        screen = render(b"\x1b[2J\x1b[H> /pmagic/bzImage old\r\x1b[2K"
                        b"> /pmagic/bzImage new\x1b[H")
        self.assertEqual(screen.command()["text"], "/pmagic/bzImage new")

    def test_cursor_home_does_not_clear_text(self):
        screen = render(b"\x1b[2J\x1b[H> /pmagic/bzImage initrd=a\x1b[H")
        self.assertEqual(screen.command()["text"], "/pmagic/bzImage initrd=a")
        self.assertEqual((screen.row, screen.column), (0, 0))

    def test_delayed_wrap_command(self):
        command = "/pmagic/bzImage initrd=/pmagic/initrd.img console=ttyS0,115200n8"
        screen = render(("\x1b[2J\x1b[H> " + command).encode(), columns=32, rows=8)
        self.assertEqual(screen.command()["text"], command)

    def test_cr_lf_bs_tab_cursor_and_clear(self):
        screen = render(b"\x1b[2J\x1b[Habc\bD\tZ\r\nsecond\x1b[1A\x1b[4G!"
                        b"\x1b[2;1f\x1b[2K", columns=16, rows=4)
        self.assertEqual(screen.lines()[0], "abD!    Z       ")
        self.assertEqual(screen.lines()[1], " " * 16)

    def test_unknown_or_truncated_sequences_refuse_command(self):
        for tail in (b"\x1b[42z", b"\x1b[", b"\x1b"):
            screen = render(b"\x1b[2J\x1b[H> /pmagic/bzImage a" + tail)
            self.assertFalse(screen.command()["complete"])
            self.assertTrue(screen.warnings)

    def test_no_clear_unknown_cells_refused(self):
        screen = render(b"> /pmagic/bzImage a")
        self.assertFalse(screen.command()["complete"])

    def test_clear_without_cursor_position_is_uncertain(self):
        screen = render(b"\x1b[2J> /pmagic/bzImage a")
        self.assertFalse(screen.command()["complete"])

    def test_bottom_edge_refused(self):
        screen = render(b"\x1b[2J\x1b[H> /pmagic/bzImage a", rows=1)
        self.assertFalse(screen.command()["complete"])

    def test_truncation_marker_refused(self):
        screen = render(b"\x1b[2J\x1b[H> /pmagic/bzImage args >")
        self.assertFalse(screen.command()["complete"])

    def test_last_clear_preserves_cursor_but_discards_old_text(self):
        screen = render(b"\x1b[2;1HOLD\x1b[2J\x1b[2;1H> /pmagic/bzImage new",
                        start_last_clear=True)
        self.assertEqual(screen.lines()[0].strip(), "")
        self.assertEqual(screen.command()["text"], "/pmagic/bzImage new")

    def test_sgr_and_utf8_single_width(self):
        screen = render("\x1b[2J\x1b[H\x1b[31mGrün\x1b[0m".encode())
        self.assertEqual(screen.lines()[0].rstrip(), "Grün")
        self.assertFalse(screen.warnings)

    def test_ris_recovers_from_binary_prefix(self):
        screen = render(b"\xff\x03\x1bc> /pmagic/bzImage clean", start_last_clear=True)
        self.assertEqual(screen.command()["text"], "/pmagic/bzImage clean")
        self.assertTrue(screen.warnings)

    def test_invalid_utf8_in_command_is_not_complete(self):
        screen = render(b"\x1bc> /pmagic/bzImage broken=\xff")
        self.assertFalse(screen.command()["complete"])

    def test_unrelated_next_line_is_not_joined(self):
        screen = render(b"\x1bc> /pmagic/bzImage a\r\nUNRELATED TEXT")
        self.assertFalse(screen.command()["complete"])

    def test_explicit_linux_editor_stops_before_old_countdown(self):
        command = (".linux /pmagic/bzImage max_loop=256 edd=on vga=normal "
                   "initrd=/pmagic/initrd.img,/pmagic/fu.img,/pmagic/m.img")
        redraw = "> " + command[:78] + "\r\n" + command[78:]
        data = ("\x1bc\x1b[20;25HAutomatic boot in 20 seconds..."
                "\x1b[18;1H" + redraw + "\x1b[18;3H").encode()
        screen = render(data)
        self.assertEqual(screen.command()["text"], command)
        self.assertIn("Automatic boot", screen.lines()[19])

    def test_explicit_linux_short_row_cursor_elsewhere_is_refused(self):
        screen = render(b"\x1bc> .linux /pmagic/bzImage a\x1b[20;1HOLD")
        self.assertFalse(screen.command()["complete"])

    def test_explicit_linux_does_not_clear_prior_malformed_warning(self):
        screen = render(b"\x1bc\x1b[42z\x1b[18;1H> .linux /pmagic/bzImage a"
                        b"\x1b[18;3H")
        self.assertFalse(screen.command()["complete"])
        self.assertIn("unhandled", screen.command()["reason"])

    def test_explicit_linux_column80_space_is_not_short_row(self):
        prefix = ".linux /pmagic/bzImage "
        command = prefix + "x" * (77 - len(prefix)) + " console=ttyS0"
        self.assertEqual(command[77], " ")
        data = ("\x1bc> " + command[:78] + "\r\n" + command[78:] + "\x1b[1;3H").encode()
        self.assertEqual(render(data).command()["text"], command)


if __name__ == "__main__":
    unittest.main()
