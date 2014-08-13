#!/user/python

"""
This script is used to visualize the timeline of threads
"""

import sys

MAX_THREAD = 4
NO_OP = "-"
threads_timelines = [[] for i in range(MAX_THREAD)]


if len(sys.argv) < 2:
  print "wrong argument"
  sys.exit()


file_obj = open(sys.argv[1])

try:
  lines = file_obj.readlines()
finally:
  file_obj.close()


time = 0
for line in lines:
  if line.startswith("#"):
    # Strip out the '#' character and whitespaces
    command = line[1:].strip()
    # We can recogenized the pair: (command_type: thread_id)
    atoms = command.split(":")
    if len(atoms) > 1:
      command_type = atoms[0]
      thread_id = int(atoms[1].strip())
      # For the missing time slot, we fill in NO_OP
      for i in range(MAX_THREAD):
        if thread_id == i:
          threads_timelines[i].append(command_type)
        else:
          threads_timelines[i].append(NO_OP)
      time += 1
    else:
      print atoms  # Print out the atoms we can not parse
  else:
    # Print out the sentences startswith non-#
    print line


'''
for i in range(MAX_THREAD):
  if len(threads_timelines[i]) == 0: continue
  print threads_timelines[i]
  print '\n'
'''

for t in range(time):
  print t,
  for i in range(MAX_THREAD):
    print threads_timelines[i][t], 
    print '\t\t\t',
  print "\n",
