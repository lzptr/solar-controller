#!/usr/bin/env python3
"""Rebuild the printable files from solar_controller_enclosure.scad.

Writes stl/*.stl and solar_controller_enclosure.3mf. In the 3MF the top shell
is one object with two parts (shell + artwork inlay), so the artwork can get
its own filament in Bambu Studio. Needs openscad on PATH.
"""
import re
import subprocess
import tempfile
import uuid
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCAD = HERE / "solar_controller_enclosure.scad"
STLS = {"base": "base_plate", "shell": "top_shell", "inlay": "artwork_inlay", "knob": "knob"}


def openscad(part, out, *extra):
    subprocess.run(["openscad", "--backend=manifold", *extra, "-D", f'part="{part}"',
                    "-o", str(out), str(SCAD)], check=True, capture_output=True)


def max_z(model, oid):
    body = re.search(rf'<object id="{oid}".*?</object>', model, flags=re.S).group(0)
    return max(float(z) for z in re.findall(r' z="([-\d.e]+)"', body))


def write_3mf(src, dst):
    zin = zipfile.ZipFile(src)
    model = zin.read("3D/3dmodel.model").decode()
    ids = re.findall(r'<object id="(\d+)" name="OpenSCAD Model \d+"', model)
    assert len(ids) == 4, ids
    # identify the parts by height: inlay 0.6, base 8, knob 11, shell 22
    inlay, base, knob, shell = sorted(ids, key=lambda oid: max_z(model, oid))
    for oid, name in ((base, "base_plate"), (shell, "shell"), (inlay, "artwork"), (knob, "knob")):
        model = re.sub(rf'(<object id="{oid}" name=")[^"]*"', rf'\g<1>{name}"', model)

    def u():
        return str(uuid.uuid4())

    top = str(max(map(int, ids)) + 1)
    model = model.replace(
        "\t</resources>",
        f'\t\t<object id="{top}" name="top_shell" type="model" p:UUID="{u()}">\n'
        f"\t\t\t<components>\n"
        f'\t\t\t\t<component objectid="{shell}" p:UUID="{u()}"/>\n'
        f'\t\t\t\t<component objectid="{inlay}" p:UUID="{u()}"/>\n'
        f"\t\t\t</components>\n"
        f"\t\t</object>\n"
        f"\t</resources>", 1)
    items = "".join(f'\t\t<item objectid="{oid}" partnumber="{name}" p:UUID="{u()}"/>\n'
                    for oid, name in ((base, "base_plate"), (top, "top_shell"), (knob, "knob")))
    model = re.sub(r"(<build[^>]*>\n).*?(\t</build>)", lambda m: m.group(1) + items + m.group(2),
                   model, count=1, flags=re.S)

    with zipfile.ZipFile(dst, "w", zipfile.ZIP_DEFLATED) as zout:
        for info in zin.infolist():
            data = model.encode() if info.filename == "3D/3dmodel.model" else zin.read(info.filename)
            zout.writestr(info, data)


def write_check_3mf(src, dst):
    """All parts in assembled position as one object, so a slicer keeps them aligned."""
    zin = zipfile.ZipFile(src)
    model = zin.read("3D/3dmodel.model").decode()
    ids = re.findall(r'<object id="(\d+)" name="OpenSCAD Model \d+"', model)
    names = ["base_plate", "top_shell", "artwork", "knob", "pcb"]   # order of part="check"
    assert len(ids) == len(names), ids
    for oid, name in zip(ids, names):
        model = re.sub(rf'(<object id="{oid}" name=")[^"]*"', rf'\g<1>{name}"', model)
    top = str(max(map(int, ids)) + 1)
    comps = "".join(f'\t\t\t\t<component objectid="{oid}" p:UUID="{uuid.uuid4()}"/>\n' for oid in ids)
    model = model.replace(
        "\t</resources>",
        f'\t\t<object id="{top}" name="assembly_check" type="model" p:UUID="{uuid.uuid4()}">\n'
        f"\t\t\t<components>\n{comps}\t\t\t</components>\n\t\t</object>\n\t</resources>", 1)
    item = f'\t\t<item objectid="{top}" partnumber="assembly_check" p:UUID="{uuid.uuid4()}"/>\n'
    model = re.sub(r"(<build[^>]*>\n).*?(\t</build>)", lambda m: m.group(1) + item + m.group(2),
                   model, count=1, flags=re.S)
    with zipfile.ZipFile(dst, "w", zipfile.ZIP_DEFLATED) as zout:
        for info in zin.infolist():
            data = model.encode() if info.filename == "3D/3dmodel.model" else zin.read(info.filename)
            zout.writestr(info, data)


def main():
    (HERE / "stl").mkdir(exist_ok=True)
    for part, name in STLS.items():
        openscad(part, HERE / "stl" / f"{name}.stl")
    with tempfile.TemporaryDirectory() as tmp:
        raw = Path(tmp) / "print.3mf"
        openscad("print", raw, "--enable=lazy-union")
        write_3mf(raw, HERE / "solar_controller_enclosure.3mf")
        if (HERE / "ref" / "pcb.stl").exists():
            raw = Path(tmp) / "check.3mf"
            openscad("check", raw, "--enable=lazy-union")
            write_check_3mf(raw, HERE / "ref" / "assembly_check.3mf")
    print("wrote stl/, solar_controller_enclosure.3mf and ref/assembly_check.3mf")


if __name__ == "__main__":
    main()
