Import("env")
import os

def erase_flash_before_upload(source, target, env):
    os.system("python -m esptool --chip esp32c3 erase_flash")

env.AddPreAction("upload", erase_flash_before_upload)