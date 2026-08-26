"""Compile an ignored JSON device profile into a private build header."""
from __future__ import print_function

import json
import os

from SCons.Script import Import

Import("env")

def fail(message):
    print("DeviceFramework profile error: {}".format(message))
    env.Exit(1)

def option(name):
    try:
        return env.GetProjectOption(name)
    except Exception:
        return None

profile_path = option("custom_device_profile")
if not profile_path:
    # A profile is optional. Normal builds never see a generated header.
    Return()

profile_path = env.subst(profile_path)
if not os.path.isabs(profile_path):
    profile_path = os.path.join(env.subst("$PROJECT_DIR"), profile_path)
if not os.path.isfile(profile_path):
    fail("custom_device_profile does not exist: {}".format(profile_path))

try:
    with open(profile_path, "r") as profile_file:
        document = json.load(profile_file)
except (IOError, ValueError) as exc:
    fail("cannot read {}: {}".format(profile_path, exc))

if not isinstance(document, dict) or document.get("format") != 1:
    fail("profile format must be integer 1")

def text(value, name, required=False):
    if value is None and not required:
        return ""
    if not isinstance(value, str) or (required and not value):
        fail("{} must be {}string".format(name, "a non-empty " if required else "a "))
    if any(ord(character) < 32 or ord(character) > 126 for character in value):
        fail("{} must contain printable ASCII only".format(name))
    return value

application = text(document.get("application"), "application", True)
profile = document.get("profile")
if not isinstance(profile, dict):
    fail("profile must be an object")
profile_id = text(profile.get("id"), "profile.id", True)
revision = profile.get("revision")
if not isinstance(revision, int) or revision < 1 or revision > 4294967295:
    fail("profile.revision must be an integer from 1 to 4294967295")
policy = text(profile.get("policy"), "profile.policy", True)
if policy not in ("bootstrap", "reconcile"):
    fail("profile.policy must be bootstrap or reconcile")

wifi = document.get("wifi", {})
if not isinstance(wifi, dict):
    fail("wifi must be an object")
wifi_ssid = text(wifi.get("ssid"), "wifi.ssid")
wifi_password = text(wifi.get("password"), "wifi.password")
if wifi_password and not wifi_ssid:
    fail("wifi.password requires wifi.ssid")

device_password = text(document.get("device_password"), "device_password")
if device_password and not 8 <= len(device_password) <= 31:
    fail("device_password must be empty or 8-31 characters")

# The profile is the only source for the shared OTA password. Endpoint and
# transport settings remain project-local PlatformIO options, while espota gets
# its authentication flag from the same device_password compiled into firmware.
if option("upload_protocol") == "espota" and device_password:
    configured_upload_flags = option("upload_flags") or ""
    if "--auth" in str(configured_upload_flags):
        fail("remove espota --auth from PlatformIO config; use profile device_password")
    env.Append(UPLOAD_FLAGS=["--auth={}".format(device_password)])

parameters = document.get("parameters", {})
if not isinstance(parameters, dict):
    fail("parameters must be an object")
parameter_items = []
for parameter_id in sorted(parameters):
    if not isinstance(parameter_id, str) or not parameter_id.isalnum():
        fail("parameter IDs must be alphanumeric")
    parameter_value = text(parameters[parameter_id], "parameters.{}".format(parameter_id))
    parameter_items.append((parameter_id, parameter_value))

def literal(value):
    return json.dumps(value, ensure_ascii=True)

build_dir = env.subst("$BUILD_DIR")
include_dir = os.path.join(build_dir, "deviceframework-profile")
if not os.path.isdir(include_dir):
    os.makedirs(include_dir)
header_path = os.path.join(include_dir, "DeviceFrameworkLocalProfile.h")

lines = [
    "#ifndef DEVICEFRAMEWORK_LOCAL_PROFILE_H",
    "#define DEVICEFRAMEWORK_LOCAL_PROFILE_H",
    "#define DEVICEFRAMEWORK_PROFILE_APPLICATION " + literal(application),
    "#define DEVICEFRAMEWORK_PROFILE_ID " + literal(profile_id),
    "#define DEVICEFRAMEWORK_PROFILE_REVISION {}UL".format(revision),
    "#define DEVICEFRAMEWORK_PROFILE_POLICY DEVICEFRAMEWORK_PROFILE_{}".format(policy.upper()),
    "#define DEVICEFRAMEWORK_PROFILE_WIFI_SSID " + literal(wifi_ssid),
    "#define DEVICEFRAMEWORK_PROFILE_WIFI_PASSWORD " + literal(wifi_password),
    "#define DEVICEFRAMEWORK_PROFILE_DEVICE_PASSWORD " + literal(device_password),
    "static const DeviceFrameworkProvisionedParameter DEVICEFRAMEWORK_PROFILE_PARAMETERS[] = {",
]
if parameter_items:
    for parameter_id, parameter_value in parameter_items:
        lines.append("    {" + literal(parameter_id) + ", " + literal(parameter_value) + "},")
else:
    # C++ has no portable zero-length arrays; the count remains zero.
    lines.append("    {\"\", \"\"},")
lines.extend([
    "};",
    "#define DEVICEFRAMEWORK_PROFILE_PARAMETER_COUNT {}".format(len(parameter_items)),
    "#endif",
    "",
])
with open(header_path, "w") as header_file:
    header_file.write("\n".join(lines))

env.Append(CPPPATH=[include_dir])
env.Append(CPPDEFINES=[("DEVICEFRAMEWORK_HAS_LOCAL_PROFILE", 1)])
print("DeviceFramework: using local profile {}".format(profile_path))
