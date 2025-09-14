import datetime
import os
import sys


def log_var(name: str, value):
    value_str = str(value)
    if len(value_str) > 100:
        value_str = "\n" + value_str + "\n"
    print(f"[DEBUG] ${name} = \"{value_str}\";")


def log_error(message: str):
    print("[ERROR] " + message);


def log_error_and_exit(message: str):
    log_error(message)
    log_end()
    sys.exit(1)


def log_start():
    print(f"[STATUS] LOG START - {datetime.datetime.now().strftime("%Y-%m-%d_%H:%M:%S")}")


def log_end():
    print(f"[STATUS] LOG END - {datetime.datetime.now().strftime("%Y-%m-%d_%H:%M:%S")}")


def log_status(message: str):
    print("[STATUS] " + message)


def log_warning(message: str):
    print("[WARNING] " + message)


log_start()

execpath = sys.argv[0]
log_var("execpath", execpath)

filepath = os.path.abspath(sys.argv[1])
log_var("filepath", filepath)

padding = sys.argv[2]
log_var("padding", padding)

try:
    with open(filepath) as f:
        code = f.read()
except:
    log_error_and_exit("Could not open file for reading.")

try:
    with open(filepath + ".bk", 'w') as f:
        f.write(code)
except:
    log_error_and_exit("Could not backup file.")

sbox_index = code.find("static const uint8_t sbox[256] = {")
if sbox_index == -1:
    log_error_and_exit("Could not find the sbox in code.")

offset = 512
log_var("code_near_sbox", code[max(0, sbox_index - offset): min(sbox_index + offset, len(code))])

patch_prefix = ("#pragma GCC push_options\n"
                "#pragma GCC optimize(\"O0\") // the compile must not optimize away this unused var\n")
#                 #define PADDING_LENGTH {padding}\n
patch_postfix = ("volatile static const uint8_t PADDING[PADDING_LENGTH] = {0};\n"
                 "#pragma GCC pop_options\n")
patch = patch_prefix + f"#define PADDING_LENGTH {padding}\n" + patch_postfix
log_var("patch", patch)

found_prefix = code.find(patch_prefix)
found_postfix = code.find(patch_postfix)
if found_prefix != -1 and found_postfix != -1 and found_prefix < found_postfix < sbox_index:
    log_warning("Patch already exists, removing it before applying the new one.")
    code = code[0:found_prefix] + code[found_postfix + len(patch_postfix):]
    sbox_index = found_prefix

if int(padding) == 0:
    log_status("Zero padding, no patch code will actually be inserted.")
    patch = ""

patched_code = code[0:sbox_index] + patch + code[sbox_index:]
log_var("patched_code_near_sbox", patched_code[max(0, sbox_index - offset): min(sbox_index + len(patch) + offset,
                                                                                len(patched_code))])
try:
    with open(filepath, 'w') as f:
        f.write(patched_code)
except:
    log_error_and_exit("Could not open file for overwriting.")

log_status("Patch successful.")
log_end()
