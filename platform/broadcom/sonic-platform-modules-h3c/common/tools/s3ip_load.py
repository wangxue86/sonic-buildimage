#!/usr/bin/python
# -*- coding: UTF-8 -*-
import json
import os

def read_file(path):
    '''
    Read file content
    '''
    try:
        with open(path, "r") as temp_read:
            value = temp_read.read()
    except Exception as error:
        #print("unable to read %s file , Error: %s" % (path, str(error)))
        return False
    return value

def ln_s_pro(s3ip_dir, odm_dir, sysfs_item):
    if "dir" == sysfs_item['type']:
        if sysfs_item.has_key("number"):
            if "unsure" == sysfs_item["odm_path"] and "eth{}/" == sysfs_item["s3ip_path"]:
                number = 0
                for tmp_dir in os.listdir(odm_dir):
                    tmp_odm_dir = odm_dir + tmp_dir
                    if False == os.path.islink(tmp_odm_dir) and True == os.path.isdir(tmp_odm_dir):
                        number = number + 1
                        index = tmp_odm_dir[tmp_odm_dir.rfind("GE"):][2:]
                        if -1 != index.find('-'):
                            index_arr = index.split('-')
                            index = (int(index_arr[0]) - 1) * 32 + int(index_arr[1])
                        elif -1 != index.find('_'):
                            index_arr = index.split('_')
                            index = (int(index_arr[0]) - 1) * 32 + int(index_arr[1])
                        tmp_s3ip_dir = s3ip_dir + sysfs_item["s3ip_path"].format(index)
                        if not os.path.exists(tmp_s3ip_dir):
                            os.mkdir(tmp_s3ip_dir)
                        for item in sysfs_item['path']:
                            ln_s_pro(tmp_s3ip_dir, tmp_odm_dir + "/", item)
            else:
                if sysfs_item.has_key("number_type"):
                    if "int" == sysfs_item["number_type"]:
                        number = int(sysfs_item["number"])
                    elif "path" == sysfs_item["number_type"]:
                        number = int(read_file(sysfs_item["number"]))
                    else:
                        print("number_type ERROR!")
                else:
                    tmp_num_path = odm_dir + sysfs_item["number"]
                    number = int(read_file(tmp_num_path))
                for i in range(0, number):
                    s3ip_index = i + int(sysfs_item['s3ip_start_num'])
                    tmp_s3ip_dir = s3ip_dir + sysfs_item['s3ip_path'].format(s3ip_index)
                    if not os.path.exists(tmp_s3ip_dir):
                        os.mkdir(tmp_s3ip_dir)
                    if " " == sysfs_item['odm_path']:
                        tmp_odm_dir = odm_dir
                    else:
                        odm_index = i + int(sysfs_item['odm_start_num'])
                        tmp_odm_dir = odm_dir + sysfs_item['odm_path'].format(odm_index)
                    for item in sysfs_item['path']:
                        ln_s_pro(tmp_s3ip_dir, tmp_odm_dir, item)
        else:
            tmp_s3ip_dir = s3ip_dir + sysfs_item['s3ip_path']
            if not os.path.exists(tmp_s3ip_dir):
                os.mkdir(tmp_s3ip_dir)
            tmp_odm_dir = odm_dir + sysfs_item['odm_path']
            for item in sysfs_item['path']:
                ln_s_pro(tmp_s3ip_dir, tmp_odm_dir, item)

    elif "path" == sysfs_item['type']:
        tmp_s3ip_dir = s3ip_dir + sysfs_item['s3ip_path']
        tmp_odm_dir = odm_dir + sysfs_item['odm_path']
        if os.path.exists(tmp_odm_dir):
            cmd = "sudo ln -s " + tmp_odm_dir + " " + tmp_s3ip_dir
            os.system(cmd)
        #else:
            #print(tmp_odm_dir + " not exist!")

    else:
        print("sysfs_path['type'] ERROR!")
        return

def sysfs_dir_pro(sysfs_item):
    if "path" == sysfs_item['type']:
        cmd = "sudo ln -s " + sysfs_item['odm_path'] + " " + sysfs_item['s3ip_path']
        os.system(cmd)
    elif "dir" == sysfs_item['type']:
        if not os.path.exists(sysfs_item['s3ip_path']):
            os.mkdir(sysfs_item['s3ip_path'])
        for item in sysfs_item['path']:
            ln_s_pro(sysfs_item['s3ip_path'], sysfs_item['odm_path'], item)
    else:
        print("sysfs_path['type'] ERROR!")


def exec_cmd(cmd_str):
    with os.popen(cmd_str) as f:
        info = f.read()
    return info

def plugins_pmon_file():
    file_path = "/etc/sonic/.plugins"
    cmd = "sudo rm -rf " + file_path + ";sudo mkdir -p " + file_path
    os.system(cmd)

    info = exec_cmd("sudo dmidecode -t memory")
    with open('/etc/sonic/.plugins/dmidecode_memory', 'w+') as fd:
        fd.write(info)

    info = exec_cmd("sudo dmidecode -s bios-vendor")
    with open('/etc/sonic/.plugins/misc_bios_vendor', 'w+') as fd:
        fd.write(info)

    info = exec_cmd("sudo dmidecode -s bios-version")
    with open('/etc/sonic/.plugins/misc_bios_version', 'w+') as fd:
        fd.write(info)
        
    info = exec_cmd("cat /host/machine.conf | grep onie_version")
    with open('/etc/sonic/.plugins/misc_onie_version', 'w+') as fd:
        fd.write(info)


if __name__ == '__main__':
    os.system("sudo rm -rf /sys_switch;sudo mkdir -p  /sys_switch")

    with open('/usr/local/bin/s3ip_sysfs_conf.json', 'r') as jsonfile:
        json_string = json.load(jsonfile)
        for sysfs_dir in json_string['s3ip_syfs_paths']:
            sysfs_dir_pro(sysfs_dir)

    plugins_pmon_file()

    #os.system("tree -l /sys_switch")

