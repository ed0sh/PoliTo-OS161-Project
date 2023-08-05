def dbos161
  dir ../src/kern/compile/DUMBVM
  target remote unix:.sockets/gdb
end

def lab3os161
  dir ../src/kern/compile/LAB3_SYNCH
  target remote unix:.sockets/gdb
end

lab3os161
