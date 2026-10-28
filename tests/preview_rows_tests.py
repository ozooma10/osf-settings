"""Exercise the native preview exporter without Starfield or Ruffle running."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


EXPORTER = Path(sys.argv.pop(1) if len(sys.argv) > 1 else "build/preview/osfsettings-preview-rows.exe").resolve()


class PreviewRowsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="osfsettings-preview-")
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.table = self.directory / "keyboard-table.utf16"
        # Same format as the engine table, including names missing from the old alias list.
        self.table.write_bytes("F4\t0x73\t115\nL Ctrl\t0xA2\t162\n/\t0xBF\t191\n".encode("utf-16-le"))

    def export(self, document, mod="preview"):
        schema = self.directory / (mod + ".json")
        schema.write_text(document if isinstance(document, str) else json.dumps(document), encoding="utf-8")
        return subprocess.run([str(EXPORTER), str(self.table), str(schema)],
                              capture_output=True, encoding="utf-8", check=False)

    def rows(self, settings):
        result = self.export({"groups": {"General": settings}})
        self.assertEqual(result.returncode, 0, result.stderr)
        return json.loads(result.stdout)["rows"]

    def export_all(self, schemas):
        paths = []
        for mod, document in schemas:
            schema = self.directory / (mod + ".json")
            schema.write_text(json.dumps(document), encoding="utf-8")
            paths.append(str(schema))
        return subprocess.run([str(EXPORTER), str(self.table), *paths],
                              capture_output=True, encoding="utf-8", check=False)

    def test_optional_label_and_mouse_defaults(self):
        rows = self.rows([
            {"key": "mouse-name", "type": "key", "default": "Mouse4", "allowMouse": True},
            {"key": "mouse-number", "type": "key", "default": 6, "allowMouse": True, "allowUnbound": False},
            {"key": "unbound", "type": "key", "default": "UNBOUND"},
        ])
        self.assertEqual([row["title"] for row in rows], ["mouse-name", "mouse-number", "unbound"])
        self.assertEqual([row["value"] for row in rows], [5, 6, 255])
        self.assertTrue(rows[0]["allowMouse"])
        self.assertFalse(rows[1]["allowUnbound"])
        self.assertFalse(rows[2]["allowMouse"])

    def test_engine_keyboard_names(self):
        rows = self.rows([{"key": str(index), "type": "key", "default": name}
                          for index, name in enumerate(["f4", "L Ctrl", "/"])])
        self.assertEqual([row["value"] for row in rows], [0x73, 0xA2, 0xBF])

    def test_preserves_values_and_option_order(self):
        rows = self.rows([
            {"key": "integer", "type": "int", "default": 9223372036854775807},
            {"key": "text", "type": "string", "default": "MiXeD 日本語 <&>"},
            {"key": "choice", "type": "enum", "default": "z", "options": {"z": "Last", "a": ""}},
            {"key": "toggle", "type": "bool", "default": False, "requires": "restart"},
        ])
        self.assertEqual(rows[0]["value"], "9223372036854775807")
        self.assertFalse(rows[0]["editable"])
        self.assertEqual(rows[1]["value"], "MiXeD 日本語 <&>")
        self.assertEqual(rows[2]["options"], [{"value": "z", "label": "Last"}, {"value": "a", "label": "a"}])
        self.assertIs(rows[3]["value"], False)
        self.assertTrue(rows[3]["requiresRestart"])

    def test_slider_coordinates_and_read_only_ranges(self):
        rows = self.rows([
            {"key": "fraction", "type": "float", "default": 0.0, "min": -0.5, "max": 1, "step": 0.2},
            {"key": "unbounded", "type": "float", "default": 0.0},
            {"key": "wide", "type": "int", "default": 0, "min": 0, "max": 4294967296},
            {"key": "integer", "type": "int", "default": 2, "min": 0, "max": 10},
        ])
        self.assertEqual([rows[0][field] for field in
                          ("sliderMinimum", "sliderMaximum", "sliderStep", "sliderScale", "sliderSteps", "decimals")],
                         [-5, 10, 2, 10, 8, 1])
        self.assertFalse(rows[1]["editable"])
        self.assertEqual(rows[1]["decimals"], -1)
        self.assertFalse(rows[2]["editable"])
        self.assertTrue(rows[3]["editable"])

    def test_empty_enum_values_preserve_labels_defaults_and_editability(self):
        for default in ["", "freefly"]:
            with self.subTest(default=default):
                row = self.rows([{
                    "key": "launch.camera", "type": "enum", "default": default,
                    "options": {"": "Use scene", "freefly": "Free fly"},
                }])[0]
                self.assertEqual(row["type"], "enum")
                self.assertEqual(row["value"], default)
                self.assertEqual(row["defaultValue"], default)
                self.assertEqual(row["options"], [
                    {"value": "", "label": "Use scene"},
                    {"value": "freefly", "label": "Free fly"},
                ])
                self.assertTrue(row["editable"])
        only = self.rows([{
            "key": "sceneOnly", "type": "enum", "default": "", "options": {"": "Use scene"},
        }])[0]
        self.assertEqual(only["value"], "")
        self.assertEqual(only["defaultValue"], "")
        self.assertEqual(only["options"], [{"value": "", "label": "Use scene"}])
        self.assertFalse(only["editable"])

    def test_production_schema_rejections(self):
        # Keep exporter-specific key resolution and one ordinary parser failure.
        # The native schema/store suites own the general validation matrix.
        invalid = [
            {"key": "mouse", "type": "key", "default": "Mouse4"},
            {"key": "mouse", "type": "key", "default": 5, "allowMouse": "true"},
            {"key": "key", "type": "key", "default": "UNBOUND", "allowUnbound": False},
            {"key": "alias", "type": "key", "default": "VK_F4"},
            {"id": "action", "type": "action"},
        ]
        for setting in invalid:
            with self.subTest(setting=setting):
                result = self.export({"groups": {"General": [setting]}})
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("preview.json:", result.stderr)
                self.assertEqual(result.stdout, "")
        for document, mod in [('{"groups":{"General":[],"General":[]}}', "preview"),
                              ({"groups": {}}, "internal")]:
            with self.subTest(mod=mod, document=document):
                self.assertNotEqual(self.export(document, mod).returncode, 0)

    def test_extension_rows_carry_their_page(self):
        def toggle(key):
            return [{"key": key, "type": "bool", "default": True}]
        result = self.export_all([
            ("basemod", {"title": "Base Mod", "groups": {"Combat": toggle("recoil")}}),
            ("basemod-combat", {"title": "Combat Extras", "extends": "basemod",
                                "groups": {"Combat": toggle("spread"), "Extras": toggle("tracer")}}),
        ])
        self.assertEqual(result.returncode, 0, result.stderr)
        rows = json.loads(result.stdout)["rows"]
        self.assertEqual([row["mod"] for row in rows], ["basemod", "basemod-combat", "basemod-combat"])
        self.assertEqual({row["page"] for row in rows}, {"basemod"})
        self.assertEqual({row["pageTitle"] for row in rows}, {"Base Mod"})
        # A shared group keeps the host's heading and names nobody; a new one names its contributor.
        self.assertEqual([row["groupTitle"] for row in rows], ["Combat", "Combat", "Extras"])
        self.assertEqual([row["groupSource"] for row in rows], ["", "", "Combat Extras"])

    def test_unresolved_extends_keeps_its_own_page(self):
        toggle = [{"key": "toggle", "type": "bool", "default": True}]
        result = self.export_all([("basemod-combat", {"title": "Combat Extras", "extends": "basemod",
                                                      "groups": {"Extras": toggle}})])
        self.assertEqual(result.returncode, 0, result.stderr)
        rows = json.loads(result.stdout)["rows"]
        self.assertEqual([row["page"] for row in rows], ["basemod-combat"])
        self.assertEqual([row["groupSource"] for row in rows], [""])

    def test_optional_extends_without_host_is_hidden(self):
        toggle = [{"key": "toggle", "type": "bool", "default": True}]
        result = self.export_all([
            ("other", {"title": "Other", "groups": {"General": toggle}}),
            ("basemod-combat", {"title": "Combat Extras", "extends": {"mod": "basemod", "optional": True},
                                "groups": {"Extras": toggle}}),
        ])
        self.assertEqual(result.returncode, 0, result.stderr)
        rows = json.loads(result.stdout)["rows"]
        self.assertEqual([row["mod"] for row in rows], ["other"])


if __name__ == "__main__":
    unittest.main()
