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
app.addEmptyMessage("Trigger",0,0)
app.setMessageWidth("Trigger", 200)
app.addEmptyMessage("EventBuilder",0,1)
app.setMessageWidth("EventBuilder", 200)
app.addTextArea("DUStatus",1,0)
#app.addScrolledTextArea("DUStatus",1,0)
def update():
  fpt = open("/tmp/daq/t3","r");
  s= fpt.read()
  app.setMessage("Trigger",s)
  #print(s)
  fpt.close()
  fpt = open("/tmp/daq/eb","r");
  s= fpt.read()
  app.setMessage("EventBuilder",s)
  #print(s)
  fpt.close()
  fpt = open("/tmp/daq/du","r");
  s= fpt.read()
  if len(s)>0:
    app.clearTextArea("DUStatus")
    app.setTextArea("DUStatus",s)
  #print(s)
  fpt.close()
  app.after(1000,update)
update()

app.go()


