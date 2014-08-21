#!/user/python

"""
This script is used to visualize the timeline of threads
It can read data from stdin or an existing file
"""

import sys


NO_OP = "-"




# How many threads will be used?
if len(sys.argv) == 2: 
  MAX_THREAD = int(sys.argv[1])
else:
  MAX_THREAD = 4

# This is the vertical thread timelines 
threads_timelines = [[] for i in range(MAX_THREAD)]


# Read-in log file from stardard input
lines = sys.stdin.readlines() 

time = 0

# Parse the log file line by line, each line represents a single time slot
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


print "\n\n\n"

#################################################### Visaul lock acquire and release
for t in range(time):
  LockCommand = False
  for i in range(MAX_THREAD):
    if "Acq" in threads_timelines[i][t] or "Rel" in threads_timelines[i][t]:
      LockCommand = True
      break
 
  if LockCommand == False: continue 

  print t,
  for i in range(MAX_THREAD):
    print threads_timelines[i][t], 
    print '\t\t\t',
  print "\n",

'''
#################################################### Visaul Token
for t in range(time):
  LockCommand = False
  for i in range(MAX_THREAD):
    if "Token" in threads_timelines[i][t] or "Reg" in threads_timelines[i][t]:
      LockCommand = True
      break
 
  if LockCommand == False: continue 

  print t,
  for i in range(MAX_THREAD):
    print threads_timelines[i][t], 
    print '\t\t\t',
  print "\n",
'''

