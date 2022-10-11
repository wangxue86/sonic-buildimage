from subprocess import check_output
import re

class Util_Base:

    def exec_cmd(self, cmd):
        try:
            output = check_output(cmd, shell=True, universal_newlines=True)
        except Exception:
            return "NA"
        return output

    def parse_output(self, pattern, buffer):
        match_obj = re.findall(pattern, buffer)
        if not match_obj:
            return "NA"
        return match_obj[0]

    def read_attr(self, path):
        """
        Read the value from the attr file

        Args:
            file path
        """
        try:
            with open(path, 'r') as fd:
                return fd.read().strip()
        except Exception:
            return "NA"

