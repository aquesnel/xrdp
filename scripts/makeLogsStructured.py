#!python3

import csv
import collections
import re
import sys

def log_iter():
    with open('/home/rsa-key-20171202-gcp-aws-cloud9/aws-cloud9-root/xrdp/xrdp-install/var/log/xrdp-sesman.log', 'r', encoding='latin_1') as sesmanLog:
        with open('/home/rsa-key-20171202-gcp-aws-cloud9/.local/share/xrdp/xrdp-chansrv.10.log', 'r', encoding='latin_1') as chansrvLog:
            with open('/home/rsa-key-20171202-gcp-aws-cloud9/aws-cloud9-root/xrdp/xrdp-install/var/log/xrdp.log', 'r', encoding='latin_1') as xrdpLog:
                lines = []
                sources = { 
                            'sesman': sesmanLog, 
                            'chansrv': chansrvLog, 
                            'xrdp': xrdpLog,
                          }
                for prog_name, f  in sources.items():
                    logEntry = StructuredLogEntry.parseLogLine(prog_name, f)
                    if logEntry:
                        lines.append((logEntry, f, prog_name))
                while lines:
                    lines.sort(reverse=True, key=lambda x: x[0].time)
                    logEntry, f, prog_name = lines.pop()
                    yield logEntry 
                    logEntry = StructuredLogEntry.parseLogLine(prog_name, f)
                    if logEntry:
                        lines.append((logEntry, f, prog_name))

def peek_line(f):
    pos = f.tell()
    line = f.readline()
    f.seek(pos)
    return line
                 
class StructuredLogEntry(object):
    @classmethod
    def parseLogLine(cls, prog_name, log_file):
        try:
            line = log_file.readline()
        except:
            print("Unexpected error reading from file {}".format(log_file.name), file=sys.stderr)
            raise
        if not line:
            return None
        fields = {'program_name': prog_name}
        end_of_entry = False
        while not end_of_entry:
            field_end = line.find(']')
            field = line[line.find('[') + 1 : field_end]
            #print("processing field '{}' from line '{}'".format(field, line.strip()), file=sys.stderr)
            if 'time' not in fields and re.match('[0-9]{8}-[0-9]{2}:[0-9]{2}:[0-9]{2}.[0-9]{3}', field):
                fields['time'] = field
            elif 'level' not in fields and re.match('ERROR|WARN|INFO|DEBUG|TRACE', field):
                fields['level'] = field
            elif 'process_id' not in fields and re.match('pid:[0-9]+ tid:[0-9]+', field):
                pid, tid = field.split(' ', maxsplit=1)
                fields['process_id'] = pid[4:]
                fields['thread_id'] = tid[4:]
            elif 'file_name' not in fields and re.match('.*\(.*:[0-9]+\)', field):
                funciton_name, rest = field.split('(', maxsplit=1)
                file_name, line_number = rest.split(':', maxsplit=1)
                fields['funciton_name'] = funciton_name
                fields['file_name'] = file_name
                fields['line_number'] = line_number[:-1]
            else:
                field = line
                #print("processing field '{}' from line '{}'".format(field, line.strip()), file=sys.stderr)
                if 'Hex Dump:' in field:
                    fields['extraTag'] = 'hex_dump'
                    end_of_dump = False
                    while not end_of_dump:
                        next_line = peek_line(log_file)
                        if re.match('[0-9a-f]{4} ', next_line):
                            field += log_file.readline()
                        else:
                            end_of_dump = True
                elif re.search('\[.+\]', field):
                    fields['extraTag'] = field[field.find('[') + 1 : field.find(']')]
                end_of_entry = True
                fields['message'] = field.strip()
            line = line[field_end + 1 : ]
                
        return StructuredLogEntry(**fields)

    def __init__(self, program_name='', time='', level='', process_id='', thread_id='', file_name='', line_number='', funciton_name='', message='', extraTag=''):
        self.program_name = program_name
        self.time = time
        self.level = level
        self.process_id = process_id
        self.thread_id = thread_id
        self.file_name = file_name
        self.line_number = line_number
        self.funciton_name = funciton_name
        self.message = message
        self.extraTag = extraTag

    def as_dict(self):
        # the retval is an ordered dict so that the insert order of keys can be 
        # used to define the order of columns in the csv file
        retval = collections.OrderedDict()
        
        retval['program_name'] = self.program_name
        retval['time'] = self.time
        retval['level'] = self.level
        retval['process_id'] = self.process_id
        retval['thread_id'] = self.thread_id
        retval['file_name'] = self.file_name
        retval['line_number'] = self.line_number
        retval['funciton_name'] = self.funciton_name
        retval['extraTag'] = self.extraTag
        retval['message'] = self.message
        return retval

def main():
    
    with open('/home/rsa-key-20171202-gcp-aws-cloud9/aws-cloud9-root/xrdp/xrdp-combined-logs.tsv', 'w', newline='') as csvfile:
        fieldnames = StructuredLogEntry().as_dict().keys()
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames, delimiter='\t')
    
        writer.writeheader()
        for entry in log_iter():
            writer.writerow(entry.as_dict())

    if False:
        with open('/home/rsa-key-20171202-gcp-aws-cloud9/aws-cloud9-root/xrdp/xrdp-combined-logs.tsv', 'r', newline='') as csvfile:
            for line in csvfile:
                print(line)

if __name__ == '__main__':
    main()