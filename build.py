import optparse
import os
import sys

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
TOOLS_DIR = os.path.join(ROOT_DIR, "tools")
SRC_DIR = os.path.join(ROOT_DIR, "src")

def InitParser():
  parser = optparse.OptionParser()
  parser.add_option("-c", "--compile", dest="compile",
                    default="", help="compile target (chrome, base, net...)")
  parser.add_option("-d", "--debug", dest="debug", action="store_true",
                    default=False, help="debug mode (false)")
  parser.add_option("-p", "--package", dest="package", action="store_true",
                    default=False, help="do package (false)")
  options, _ = parser.parse_args()
  return options

def InitTarget():
  if sys.platform.startswith("linux"):
    platform_list = [ "x64", "arm64", "mips64el", "loong64", "android" ]
  elif sys.platform.startswith("win"):
    platform_list = [ "x86", "x64" ]
  target_dir = os.path.join(ROOT_DIR, "platform")
  extended_platform_list = []
  if (os.path.exists(target_dir)):
    extended_platform_list = os.listdir(target_dir)
    def contains(s):
      return os.path.isdir(os.path.join(target_dir, s))
    extended_platform_list = filter(contains, extended_platform_list)
    extended_platform_list.sort()
    platform_list.extend(extended_platform_list)
  return (target_dir, platform_list, extended_platform_list)

def Status(status):
  if (bool(status)): print("\n[nano] build successful")
  else: print("\n[nano] error occured")
  sys.exit()

def Prepare(options, platform):
  if (bool(options.debug)): build_type = "debug"
  else: build_type = "release"
  if sys.platform.startswith("linux"):
    gn = os.path.join(TOOLS_DIR, "gn")
    ninja = os.path.join(TOOLS_DIR, "ninja")
    if (bool(options.debug)):
      out_folder = platform.lower() + "_" + build_type
    else:
      out_folder = platform.lower()
    out_dir = os.path.join(ROOT_DIR, "out", out_folder)
  elif sys.platform.startswith("win"):
    gn = os.path.join(TOOLS_DIR, "gn.exe")
    ninja = os.path.join(TOOLS_DIR, "ninja.exe")
    out_folder = platform.lower() + "_" + build_type
    out_dir = os.path.join(ROOT_DIR, out_folder)
  if not os.path.exists(out_dir):
    os.makedirs(out_dir)
  return (gn, ninja, out_dir)

def WriteGNArgs(options, platform, out_dir):
  additional_args = ""
  additional_args_file = ""
  if platform.startswith("android"):
    additional_args_file = os.path.join(ROOT_DIR, "args_android.gn")
  else:
    additional_args_file = os.path.join(ROOT_DIR, "args.gn")
  if os.path.exists(additional_args_file):
    with open(additional_args_file, "r") as f1:
      additional_args = f1.read()
  with open(os.path.join(out_dir, "args.gn"), "w") as f2:
    args = ""
    args += "is_component_build = " + str(options.debug).lower() + "\n"
    args += "is_debug = " + str(options.debug).lower() + "\n"
    args += "is_official_build = " + str(not options.debug).lower() + "\n"
    if platform.lower() == "android":
      args += "target_os = \"android\"" + "\n"
    else:
      args += "target_cpu = " + "\"" + platform.lower() + "\"" + "\n"
      if sys.platform.startswith("linux"):
        args += "symbol_level = " + ("2" if options.debug else "0") + "\n"
    args += "\n" + additional_args + "\n"
    f2.write(str(args))

def ExecuteGN(gn, out_dir):
  cmd = gn + " gen " + out_dir + " --root=" + SRC_DIR
  if sys.platform.startswith("win"):
    cmd += " --ide=vs --filters=//chrome --no-deps"
  ret = os.system(cmd)
  if (ret != 0): Status(False)

def ExecuteNinja(platform, ninja, out_dir, options):
  if (options.compile.strip() != ''):
    ret = os.system(ninja + " -C " + out_dir + " " + options.compile)
    if (ret != 0): Status(False)
  else:
    if platform.startswith("android"):
      ret = os.system(ninja + " -C " + out_dir + " system_webview_apk")
      if (ret != 0): Status(False)
      ret = os.system(ninja + " -C " + out_dir + " chrome_public_apk")
      if (ret != 0): Status(False)
    else:
      ret = os.system(ninja + " -C " + out_dir + " chrome")
      if (ret != 0): Status(False)

def DoPackage(ninja, out_dir, options):
  if (not options.debug):
    if sys.platform.startswith("win"):
      ret = os.system(ninja + " -C " + out_dir + " mini_installer")
      if (ret != 0): Status(False)
    elif sys.platform.startswith("linux"):
      ret = os.system(ninja + " -C " + out_dir + " stable_deb")
      if (ret != 0): Status(False)
      ret = os.system(ninja + " -C " + out_dir + " stable_rpm")
      if (ret != 0): Status(False)
  else:
    print("[nano] can not package in debug mode")
    Status(False)

def Build(options, platform, extended_platform_list, target_dir):
  if (platform not in extended_platform_list):
    (gn, ninja, out_dir) = Prepare(options, platform)
    WriteGNArgs(options, platform, out_dir)
    ExecuteGN(gn, out_dir)
    if (bool(options.package)):
      DoPackage(ninja, out_dir, options)
    else:
      ExecuteNinja(platform, ninja, out_dir, options)
  else:
    if sys.platform.startswith("linux"):
      os.environ["BUILD_TARGET"] = platform
      ret = os.system(os.path.join(target_dir, platform, "build.sh"))
      if (ret != 0): Status(False)
    elif sys.platform.startswith("win"):
      Status(False)

def main():
  options = InitParser()
  (target_dir, platform_list, extended_platform_list) = InitTarget()
  print("[nano] please enter the index of the platform you want to compile")
  for i, file in enumerate(platform_list):
    print("[nano] " + str(i) + ": " + file)
  user_input = input()
  if user_input.isdigit():
    user_input = (int)(user_input)
    if 0 <= user_input < len(platform_list):
      Build(options, platform_list[user_input], extended_platform_list, target_dir)
    else: Status(False)
  else: Status(False)
  Status(True)

if __name__ == "__main__":
  sys.exit(main())
