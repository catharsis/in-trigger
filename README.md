in-trigger
==========

A simple command-line tool to trigger commands on file changes

usage
==========

**in-trigger command [pattern]**
__________________________________________________________________________
**example** - *build project on file update, matching files ending in .c or .h*:


    [~/repos/in-trigger on master]
    % in-trigger "make" "*.[ch]" 
    [INFO] (trigger.c:129) Inotify instance initialized ...
    [INFO] (trigger.c:131) Watch added for '/home/alofgren/repos/in-trigger' ...
    Trigger (trigger.c): Executing command 'make'
    cc  -c trigger.c -o trigger.o
    cc  trigger.o -o  in-trigger
    Trigger (trigger.c): done.
__________________________________________________________________________
**example** - *run tests on file update, matching files ending in .py*:


    [~/repos/bbfs on master]
    % in-trigger "make test" "*.py"
    [INFO] (trigger.c:130) Inotify instance initialized ...
    [INFO] (trigger.c:132) Watch added for '/home/alofgren/repos/bbfs' ...
    Trigger (bbfs.py): Executing command 'make test'
    nosetests --nologcapture test/ || (tail -n 35 bbfs_test.log && exit 1);
    .
    ----------------------------------------------------------------------
    Ran 1 test in 0.203s

    OK
    Trigger (bbfs.py): done.
__________________________________________________________________________
