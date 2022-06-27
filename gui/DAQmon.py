# import the library
from appJar import gui
from array import array
import subprocess
import socket
import sys
import os
import time



app = gui("DAQ Status", "600x200")
app.setBg("white")
app.setFg("black")
app.setFont(12)
# add & configure widgets - widgets get a name, to help referencing them later
app.addEmptyMessage("Trigger")
app.setMessageWidth("Trigger", 200)
def update():
  fpt = open("/tmp/daq/t3","r");
  s= fpt.read()
  app.setMessage("Trigger",s)
  print(s)
  fpt.close()
  app.after(1000,update)
update()

app.go()


