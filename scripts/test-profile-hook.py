#!/usr/bin/env python3
"""Regression check for the library profile hook's upload-environment boundary."""

from __future__ import print_function

import contextlib
import inspect
import io
import json
import runpy
import sys
import tempfile
import types
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROFILE_HOOK = ROOT / "tools" / "platformio" / "profile.py"


class FakeEnvironment(object):
    def __init__(self, project_dir, build_dir):
        self.project_dir = str(project_dir)
        self.build_dir = str(build_dir)
        self.appended = []
        self.options = {
            "custom_device_profile": "profile.json",
            "upload_protocol": "espota",
            # A library hook must ignore even a pre-existing uploader flag:
            # direct uploader policy belongs to the project-level upload hook.
            "upload_flags": "--auth=legacy-value",
        }

    def GetProjectOption(self, name):
        return self.options.get(name)

    def subst(self, value):
        substitutions = {
            "$PROJECT_DIR": self.project_dir,
            "$BUILD_DIR": self.build_dir,
        }
        return substitutions.get(value, value)

    def Append(self, **kwargs):
        self.appended.append(kwargs)

    def Exit(self, status):
        raise AssertionError("profile hook exited with status {}".format(status))


@contextlib.contextmanager
def fake_scons(environment):
    saved_modules = {
        name: sys.modules.get(name)
        for name in ("SCons", "SCons.Script")
    }
    scons = types.ModuleType("SCons")
    scons.__path__ = []
    script = types.ModuleType("SCons.Script")

    def import_environment(name):
        if name != "env":
            raise AssertionError("unexpected SCons import: {}".format(name))
        caller = inspect.currentframe().f_back
        caller.f_globals["env"] = environment

    script.Import = import_environment
    scons.Script = script
    sys.modules["SCons"] = scons
    sys.modules["SCons.Script"] = script
    try:
        yield
    finally:
        for name, module in saved_modules.items():
            if module is None:
                sys.modules.pop(name, None)
            else:
                sys.modules[name] = module


def main():
    with tempfile.TemporaryDirectory(prefix="deviceframework-profile-hook-") as temporary:
        temporary_path = Path(temporary)
        profile_path = temporary_path / "profile.json"
        profile_path.write_text(json.dumps({
            "format": 2,
            "application": "profile-hook-fixture",
            "profile": {"id": "profile-hook", "revision": 1, "policy": "bootstrap"},
            "device_password": "fixture-password",
        }), encoding="ascii")
        environment = FakeEnvironment(temporary_path, temporary_path / "build")

        # The real hook emits only the selected profile path. Keep this test
        # quiet so a release gate cannot accidentally surface fixture detail.
        with fake_scons(environment), contextlib.redirect_stdout(io.StringIO()):
            runpy.run_path(str(PROFILE_HOOK), run_name="__profile_hook_test__")

        generated_header = temporary_path / "build" / "deviceframework-profile" / "DeviceFrameworkLocalProfile.h"
        if not generated_header.is_file():
            raise AssertionError("profile hook did not generate its private header")
        if any("UPLOAD_FLAGS" in values for values in environment.appended):
            raise AssertionError("library profile hook must not modify uploader flags")
        if not any("CPPPATH" in values for values in environment.appended):
            raise AssertionError("profile hook did not add the generated header include path")
        if not any("CPPDEFINES" in values for values in environment.appended):
            raise AssertionError("profile hook did not enable the generated profile header")

    print("DeviceFramework profile-hook boundary check passed")


if __name__ == "__main__":
    main()
