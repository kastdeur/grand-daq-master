# import the library
from appJar import gui
from array import array
import subprocess
import socket
import sys

def testPulseRange(tp):
  value = app.getEntry(tp)
  if value != None:
    if value < 0:
      value = 0
    elif value>255:
      value = 255
    app.setEntry(tp,int(value))

def VerifyRanges(tp):
#testpulse
  value = app.getEntry("Test Pulse")
  if value != None:
    value = int(app.getEntry("Test Pulse"))
    if value != 0:
      if value  < 124: value = 124
      divider  =  0
      oldrate = 1000001
      for idiv in range (1,255):
        incr = 1
        if (idiv > 224): incr = 128
        elif (idiv > 192): incr = 64
        elif (idiv > 160): incr = 32
        elif (idiv > 128): incr = 16
        elif (idiv > 96): incr = 8
        elif (idiv > 64): incr = 4
        elif (idiv > 32): incr = 2
        divider += incr
        newrate = int(1000000/divider)
        if value >= newrate and value < oldrate :
          value = newrate
        oldrate = newrate
  else:
    value = 0;
  app.setEntry("Test Pulse",int(value))
  voltl = app.getEntry("BatLow")
  if voltl == None: voltl = 5
  if voltl < 5: voltl = 5
  volth = app.getEntry("BatHigh")
  if volth == None: volth = 15
  if volth > 15: volth = 15
  if voltl > (volth - 2):
    if voltl < 7: voltl = 5
    if voltl > 12: voltl = 12
    volth = voltl + 2
  app.setEntry("BatLow",voltl)
  app.setEntry("BatHigh",volth)
# All timings
  Tover = app.getEntry("Tover")
  if Tover != None:
    Tover = int(32*int((Tover+15)/32))
    if Tover < 64:
      Tover = 64
    elif Tover>32000:
      Tover = 32000
  else:
    Tover = 64
  app.setEntry("Tover",int(Tover))
  for ch in range(1,5):
    TPre = app.getEntry("C"+str(ch)+"TPre")
    if TPre == None:
      TPre = 64
    TPre = int(32*int((TPre+15)/32))
    if TPre < 64:
      TPre = 64
    elif TPre>32000:
      TPre = 32000
    app.setEntry("C"+str(ch)+"TPre",int(TPre))
    tsum = Tover+TPre
    if tsum > 32704:
      TPre = int(TPre+32704-tsum)
      app.setEntry("C"+str(ch)+"TPre",int(TPre))
    TPost = app.getEntry("C"+str(ch)+"TPost")
    if TPost != None:
      TPost = int(32*int((TPost+15)/32))
      if TPost < 64:
        TPost = 64
      elif TPost>32000:
        TPost = 32000
    else:
      TPost = 64
    app.setEntry("C"+str(ch)+"TPost",int(TPost))
    tsum = Tover+TPost+TPre
    if tsum > 32768:
      TPost = int(TPost+32768-tsum)
      app.setEntry("C"+str(ch)+"TPost",int(TPost))
# Other channel dependent parameters
  for ch in range(1,5):
    Intt = app.getEntry("C"+str(ch)+"Int")
    if Intt != None:
      if Intt < 0:
        Intt = 0
      elif Intt > 15:
        Intt = 15
    else:
      Intt = 0
    app.setEntry("C"+str(ch)+"Int",int(Intt))
    value = app.getEntry("C"+str(ch)+"TrigTprev")
    if value != None:
      if value < 0:
        value = 0
      elif value > 1020:
        value = 1020
    else:
      value = 0
    app.setEntry("C"+str(ch)+"TrigTprev",int(4*int((value+3)/4)))
    value = app.getEntry("C"+str(ch)+"TrigTcmax")
    if value != None:
      if value < 0:
        value = 0
      elif value > 1020:
        value = 1020
    else:
      value = 0
    app.setEntry("C"+str(ch)+"TrigTcmax",int(4*int((value+3)/4)))
    value = app.getEntry("C"+str(ch)+"TrigTper")
    if value != None:
      if value < 0:
        value = 0
      elif value > 4080:
        value = 4080
    else:
      value = 0
    app.setEntry("C"+str(ch)+"TrigTper",int(16*int((value+7)/16)))
    value = app.getEntry("C"+str(ch)+"TrigNcmin")
    if value != None:
      if value < 0:
        value = 0
      elif value > 255:
        value = 255
    else:
      value = 0
    app.setEntry("C"+str(ch)+"TrigNcmin",int(value))
    value = app.getEntry("C"+str(ch)+"TrigNcmax")
    vmin = app.getEntry("C"+str(ch)+"TrigNcmin")
    if value != None:
      if value < vmin:
        value = vmin
      elif value > 255:
        value = 255
    else:
      value = vmin
    app.setEntry("C"+str(ch)+"TrigNcmax",int(value))
    value = app.getEntry("C"+str(ch)+"TrigQmin")
    if value != None:
      if value < 0:
        value = 0
      elif value > 255:
        value = 255
    else:
      value = 0
    app.setEntry("C"+str(ch)+"TrigQmin",int(value))
    value = app.getEntry("C"+str(ch)+"TrigQmax")
    vmin = app.getEntry("C"+str(ch)+"TrigQmin")
    if value != None:
      if value < vmin:
        value = vmin
      elif value > 255:
        value = 255
    else:
      value = vmin
    app.setEntry("C"+str(ch)+"TrigQmax",int(value))
    GaindB = app.getEntry("C"+str(ch)+"Gain")
    if GaindB < -14: GaindB = -14
    if GaindB > 23.5: GaindB = 23.5
    app.setEntry("C"+str(ch)+"Gain",GaindB)


def send_daq(msgid):
    msg = array('H',[4,2,msgid,0x4541,0x4152]) # h = signed short, H = unsigned short
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        # Connect to server and send data
        sock.connect(("localhost", 5010))# start the GUI
        print(msg,"\n")
        sock.sendall(msg)
        sock.close();

def press(button):
    if button == "Initialize":
        send_daq(402)
    elif button == "Start run":#
        send_daq(403)
    elif button == "Stop run":
        #start comms
        send_daq(404)
    elif button == "Configure CDAQ":
        app.showSubWindow("DAQ Configuration")
    elif button == "Configure DU":
        app.showSubWindow("DigitalModule")
    elif button == "Blue":
        if(app.getButton("Blue") == "Blue"):
          app.setBg("Blue");
          app.setButton("Blue","Black")
        else:
          app.setBg("Black");
          app.setButton("Blue","Blue")

    else:
        app.stop()

def confbut(button):
  if button == "Read DAQ Configuration file":
    statlist=[]
    ticklist=[]
    fname = app.getEntry("conffile")
    fp=open(fname,"r")
    for line in fp:
      words = line.split()
      if words[0] == "DU":
#        print(words[1])
        statlist.append(words[1]+" " +words[2])
        ticklist.append(True)
      elif words[0] == "#DU":
        statlist.append(words[1]+" " +words[2])
        ticklist.append(False)
      elif words[0] == "EBRUN":
        print(words[1])
        app.setEntry("Next Run",words[1])
      elif words[0] == "EBMODE":
        if words[1] == "0":
          app.setOptionBox("Run Mode","Physics",True)
        else:
          app.setOptionBox("Run Mode","Test",True)
      elif words[0] == "EBSIZE":
        app.setEntry("File Size",words[1])
      elif words[0] == "EBDIR":
        app.setEntry("Data Folder",words[1])
        app.setEntryWidth("Data Folder",len(app.getEntry("Data Folder")))
      elif words[0] == "T3RAND":
        app.setEntry("Random",words[1])
#      else :
#        print(words[0])
    fp.close()
    app.changeOptionBox("Stations",statlist)
    index=0
    for tick in ticklist:
      app.setOptionBox("Stations",statlist[index],tick)
      index +=1
  elif button == "Write DAQ Configuration file":
    fname = app.getEntry("conffile")
    print("Trying to write to "+fname)
    fp=open(fname,"w")
    fp.write("#Automatically generated from Gui\n")
    boxlist=app.getOptionBox("Stations")
    for stat,tick in boxlist.items():
      if tick == True:
        fp.write("DU "+stat+"\n")
      else:
        fp.write("#DU "+stat+"\n")
    fp.write("EBRUN "+str(int(app.getEntry("Next Run")))+"\n")
    if(app.getOptionBox("Run Mode") == "Physics"):
      fp.write("EBMODE 0\n")
    else:
      fp.write("EBMODE 2\n")
    fp.write("EBSIZE "+str(int(app.getEntry("File Size")))+"\n")
    fp.write("EBDIR "+app.getEntry("Data Folder")+"\n")
    fp.write("T3RAND "+str(int(app.getEntry("Random")))+"\n")
    fp.close();
  elif button == "Add Station":
    statlist=[]
    ticklist=[]
    boxlist=app.getOptionBox("Stations")
    for stat,tick in boxlist.items():
      statlist.append(stat)
      ticklist.append(tick)
      statlist.append(app.getEntry("IP adress")+" "+app.getEntry("Port"))
      ticklist.append(True)
      app.changeOptionBox("Stations",statlist)
      index=0
      for tick in ticklist:
        print(tick)
        app.setOptionBox("Stations",statlist[index],tick)
        index +=1
  elif button == "Remove Station":
    statlist=[]
    ticklist=[]
    boxlist=app.getOptionBox("Stations")
    for stat,tick in boxlist.items():
      if stat != app.getEntry("IP adress")+" "+app.getEntry("Port"):
        statlist.append(stat)
        ticklist.append(tick)
        app.changeOptionBox("Stations",statlist)
        index=0
        for tick in ticklist:
          print(tick)
          app.setOptionBox("Stations",statlist[index],tick)
          index +=1

def DUfile(button):
  if button == "Read DU Configuration file":
    print("Reading")
    chval = 0
    fname = app.getEntry("DUconffile")
    fp=open(fname,"r")
    for line in fp:
      if line[:1] != '#':
        words = line.split()
        try:
          axi = int(words[0])
          address = int(words[1],0)
          if address == 0:
            value =int(words[2],0)
            if (value & 1<<0) == 0:
              app.setOptionBox("Configuration","Enable DAQ",False)
            else:
              app.setOptionBox("Configuration","Enable DAQ",True)
            if (value & 1<<1) == 0:
              app.setOptionBox("Configuration","1PPS",False)
            else:
              app.setOptionBox("Configuration","1PPS",True)
            if (value & 1<<6) == 0:
              app.setOptionBox("Configuration","Fake ADC",False)
            else:
              app.setOptionBox("Configuration","Fake ADC",True)
            if (value & 1<<8) == 0:
              app.setOptionBox("Configuration","Filter 1",False)
            else:
              app.setOptionBox("Configuration","Filter 1",True)
            if (value & 1<<9) == 0:
              app.setOptionBox("Configuration","Filter 2",False)
            else:
              app.setOptionBox("Configuration","Filter 2",True)
            if (value & 1<<10) == 0:
              app.setOptionBox("Configuration","Filter 3",False)
            else:
              app.setOptionBox("Configuration","Filter 3",True)
            if (value & 1<<11) == 0:
              app.setOptionBox("Configuration","Filter 4",False)
            else:
              app.setOptionBox("Configuration","Filter 4",True)
            if (value & 1<<15) == 0:
              app.setOptionBox("Configuration","Auto reboot",False)
            else:
              app.setOptionBox("Configuration","Auto reboot",True)
          if address == 2:
            value =int(words[2],0)
            if (value & 1<<0) == 0:
              app.setOptionBox("Trigger","Ch 3 AND Ch 4",False)
            else:
              app.setOptionBox("Trigger","Ch 3 AND Ch 4",True)
            if (value & 1<<1) == 0:
              app.setOptionBox("Trigger","Ch 1 AND Ch 2, Ch2>Ch1",False)
            else:
              app.setOptionBox("Trigger","Ch 1 AND Ch 2, Ch2>Ch1",True)
            if (value & 1<<2) == 0:
              app.setOptionBox("Trigger","(not Ch 1) AND Ch 2",False)
            else:
              app.setOptionBox("Trigger","(not Ch 1) AND Ch 2",True)
            if (value & 1<<4) == 0:
              app.setOptionBox("Trigger","Internal",False)
            else:
              app.setOptionBox("Trigger","Internal",True)
            if (value & 1<<5) == 0:
              app.setOptionBox("Trigger","10 sec",False)
            else:
              app.setOptionBox("Trigger","10 sec",True)
            if (value & 1<<6) == 0:
              app.setOptionBox("Trigger","20 Hz",False)
            else:
              app.setOptionBox("Trigger","20 Hz",True)
            if (value & 1<<7) == 0:
              app.setOptionBox("Trigger","Ch 1 AND Ch 2",False)
            else:
              app.setOptionBox("Trigger","Ch 1 AND Ch 2",True)
            if (value & 1<<8) == 0:
              app.setOptionBox("Trigger","Channel 1",False)
            else:
              app.setOptionBox("Trigger","Channel 1",True)
            if (value & 1<<9) == 0:
              app.setOptionBox("Trigger","Channel 2",False)
            else:
              app.setOptionBox("Trigger","Channel 2",True)
            if (value & 1<<10) == 0:
              app.setOptionBox("Trigger","Channel 3",False)
            else:
              app.setOptionBox("Trigger","Channel 3",True)
            if (value & 1<<11) == 0:
              app.setOptionBox("Trigger","Channel 4",False)
            else:
              app.setOptionBox("Trigger","Channel 4",True)
          if address == 4:
            value =int(words[2],0)
            rate = 0
            chval  = value
            for ch in range(1,5):
              if (value & 1<<(ch-1)) == 0:
                app.setOptionBox("Channel "+str(ch),"Off")
            if ( value & 1<<7 ) != 0:
              idivider = value >>8
              if idivider != 0:
                divider = 0
                for idiv in range (1,idivider+1):
                  incr = 1
                  if (idiv > 224): incr = 128
                  elif (idiv > 192): incr = 64
                  elif (idiv > 160): incr = 32
                  elif (idiv > 128): incr = 16
                  elif (idiv > 96): incr = 8
                  elif (idiv > 64): incr = 4
                  elif (idiv > 32): incr = 2
                  divider += incr
                rate = int(1000000/divider)
            app.setEntry("Test Pulse",rate)
          if address == 6:
              app.setEntry("Tover",int(words[2],0)<<1)
          if address == 8:
            for ch in range(1,5):
              value = ( int(words[2],0)>>(4*(ch-1))) & 0xf
              app.setOptionBox("Channel "+str(ch),value+1,True)
              if((chval != 0) and ((chval & 1<<(ch-1)) == 0)):
                app.setOptionBox("Channel "+str(ch),"Off")
          if address == 0xC:
            volt = int(words[2],0)*(2.5*(18+91))/(18*4096)
            volt = int(10*(volt+0.05))/10
            app.setEntry("BatLow",volt)
          if address == 0xE:
            volt = int(words[2],0)*(2.5*(18+91))/(18*4096)
            volt = int(10*(volt+0.05))/10
            app.setEntry("BatHigh",volt)
          if address == 0x10 or address == 0x14 or address == 0x18 or address == 0x1C:
            app.setEntry("C"+str(axi-3)+"TPre",int(words[2],0)<<1)
          if address == 0x12 or address == 0x16 or address == 0x1A or address == 0x1E:
            app.setEntry("C"+str(axi-3)+"TPost",int(words[2],0)<<1)
          if address == 0x20 or address == 0x2C or address == 0x38 or address == 0x44:
            GainADC  = int(words[2],0)
            GaindB = (GainADC*(37.5*2.5)/4096)-14
            GaindB = int(100*(GaindB+0.005))/100
            app.setEntry("C"+str((int)((axi-5)/3))+"Gain",GaindB)
          if address == 0x22 or address == 0x2E or address == 0x3A or address == 0x46:
            app.setEntry("C"+str((int)((axi-5)/3))+"Int",int(words[2],0)>>8)
          if address == 0x24 or address == 0x30 or address == 0x3C or address == 0x48:
            app.setEntry("C"+str((int)((axi-5)/3))+"BMax",int(words[2],0))
          if address == 0x26 or address == 0x32 or address == 0x3E or address == 0x4A:
            app.setEntry("C"+str((int)((axi-5)/3))+"BMin",int(words[2],0))
          if address == 0x50 or address == 0x5C or address == 0x68 or address == 0x74:
            app.setEntry("C"+str((int)((axi-17)/3))+"TrigT1",int(words[2],0))
          if address == 0x52 or address == 0x5E or address == 0x6A or address == 0x76:
            app.setEntry("C"+str((int)((axi-17)/3))+"TrigT2",int(words[2],0))
          if address == 0x54 or address == 0x60 or address == 0x6C or address == 0x78:
            app.setEntry("C"+str((int)((axi-17)/3))+"TrigTper",16*(int(words[2],0)>>8))
            app.setEntry("C"+str((int)((axi-17)/3))+"TrigTprev",4*(int(words[2],0) & 0xFF))
          if address == 0x56 or address == 0x62 or address == 0x6E or address == 0x7A:
            app.setEntry("C"+str((int)((axi-17)/3))+"TrigNcmax",(int(words[2],0)>>8))
            app.setEntry("C"+str((int)((axi-17)/3))+"TrigTcmax",4*(int(words[2],0) & 0xFF))
          if address == 0x58 or address == 0x64 or address == 0x70 or address == 0x7C:
            app.setEntry("C"+str((int)((axi-17)/3))+"TrigQmax",(int(words[2],0)>>8))
            app.setEntry("C"+str((int)((axi-17)/3))+"TrigNcmin",(int(words[2],0) & 0xFF))
          if address == 0x5A or address == 0x66 or address == 0x72 or address == 0x7E:
            app.setEntry("C"+str((int)((axi-17)/3))+"TrigQmin",(int(words[2],0) & 0xFF))
          if address == 0x80 or address == 0x90 or address == 0xA0 or address == 0xB0:
            app.setEntry("C"+str(int(axi/4-7))+"F1M",float(words[2]))
            app.setEntry("C"+str(int(axi/4-7))+"F1W",float(words[3]))
          if address == 0xC0 or address == 0xD0 or address == 0xE0 or address == 0xF0:
            app.setEntry("C"+str(int(axi/4-11))+"F2M",float(words[2]))
            app.setEntry("C"+str(int(axi/4-11))+"F2W",float(words[3]))
          if address == 0x100 or address == 0x110 or address == 0x120 or address == 0x130:
            app.setEntry("C"+str(int(axi/4-15))+"F3M",float(words[2]))
            app.setEntry("C"+str(int(axi/4-15))+"F3W",float(words[3]))
          if address == 0x140 or address == 0x150 or address == 0x160 or address == 0x170:
            app.setEntry("C"+str(int(axi/4-19))+"F4M",float(words[2]))
            app.setEntry("C"+str(int(axi/4-19))+"F4W",float(words[3]))
          if address == 0x1E0:
            app.setEntry("ExpRate",int(words[2],0))
        except:
          print("oops "+line)
    fp.close()
  elif button == "Write DU Configuration file":
    VerifyRanges(button)
    fname = app.getEntry("DUconffile")
    print("Trying to write to "+fname)
    fp=open(fname,"w")
    fp.write("#Automatically generated from Gui\n")
    boxlist = app.getOptionBox("Configuration")
    iconf = 0
    for conf,tick in boxlist.items():
      if tick == True:
        if conf == "Auto reboot":
          iconf += (1<<15)
        elif conf == "Filter 4":
          iconf += (1<<11)
        elif conf == "Filter 3":
          iconf += (1<<10)
        elif conf == "Filter 2":
          iconf += (1<<9)
        elif conf == "Filter 1":
          iconf += (1<<8)
        elif conf == "Fake ADC":
          iconf += (1<<6)
        elif conf == "1PPS":
          iconf += (1<<1)
        elif conf == "Enable DAQ":
          iconf += (1<<0)
        else:
          print("Unknown configuration parameter\n")
    fp.write("0 0x000 "+hex(iconf)+"\n")
    boxlist = app.getOptionBox("Trigger")
    iconf = 0
    for conf,tick in boxlist.items():
      if tick == True:
        if conf == "Channel 4":
          iconf += (1<<11)
        elif conf == "Channel 3":
          iconf += (1<<10)
        elif conf == "Channel 2":
          iconf += (1<<9)
        elif conf == "Channel 1":
          iconf += (1<<8)
        elif conf == "Ch1 AND Ch2":
          iconf += (1<<7)
        elif conf == "20 Hz":
          iconf += (1<<6)
        elif conf == "10 sec":
          iconf += (1<<5)
        elif conf == "Internal":
          iconf += (1<<4)
        elif conf == "(not Ch 1) AND Ch 2":
          iconf += (1<<2)
        elif conf == "Ch 1 AND Ch 2, Ch2>Ch1":
          iconf += (1<<1)
        elif conf == "Ch 3 AND Ch 4":
          iconf += (1<<0)
        else:
          print("Unknown trigger parameter\n")
    fp.write("0 0x002 "+hex(iconf)+"\n")
    iread=0
    iselector = 0
    if int(app.getEntry("Test Pulse")) != 0:
      rate = int(app.getEntry("Test Pulse"))
      if rate < 124: rate = 124
      divider  =  0
      oldrate = 1000001
      for idiv in range (1,255):
        incr = 1
        if (idiv > 224): incr = 128
        elif (idiv > 192): incr = 64
        elif (idiv > 160): incr = 32
        elif (idiv > 128): incr = 16
        elif (idiv > 96): incr = 8
        elif (idiv > 64): incr = 4
        elif (idiv > 32): incr = 2
        divider += incr
        newrate = int(1000000/divider)
        if rate >= newrate and rate < oldrate :
          iread+=(idiv<<8)
          app.setEntry("Test Pulse",newrate)
        oldrate = newrate
      iread +=(1<<7)
    for ch in range(1,5):
        if app.getOptionBox("Channel "+str(ch)) != "Off":
          iread+=(1<<(ch-1))
        if app.getOptionBox("Channel "+str(ch)) == "ADC 1":
          iselector +=(0<<(4*(ch-1)))
        elif app.getOptionBox("Channel "+str(ch)) == "ADC 2":
          iselector +=(1<<(4*(ch-1)))
        elif app.getOptionBox("Channel "+str(ch)) == "ADC 3":
          iselector +=(2<<(4*(ch-1)))
        elif app.getOptionBox("Channel "+str(ch)) == "ADC 4":
          iselector +=(3<<(4*(ch-1)))
        elif app.getOptionBox("Channel "+str(ch)) == "ADC Filtered 1":
          iselector +=(4<<(4*(ch-1)))
        elif app.getOptionBox("Channel "+str(ch)) == "ADC Filtered 2":
          iselector +=(5<<(4*(ch-1)))
        elif app.getOptionBox("Channel "+str(ch)) == "ADC Filtered 3":
          iselector +=(6<<(4*(ch-1)))
        elif app.getOptionBox("Channel "+str(ch)) == "ADC Filtered 4":
          iselector +=(7<<(4*(ch-1)))
    fp.write("1 0x004 "+hex(iread)+"\n")
    fp.write("1 0x006 "+hex(int(app.getEntry("Tover")/2))+"\n")
    fp.write("2 0x008 "+hex(iselector)+"\n")
    voltl = app.getEntry("BatLow")
    if voltl == None: voltl = 5
    if voltl < 5: voltl = 5
    volth = app.getEntry("BatHigh")
    if volth == None: volth = 15
    if volth > 15: volth = 15
    if voltl > (volth - 2):
      if voltl < 7: voltl = 5
      if voltl > 12: voltl = 12
      volth = voltl + 2
    app.setEntry("BatLow",voltl)
    app.setEntry("BatHigh",volth)
    value = int(voltl*(18*4096)/(2.5*(18+91)))
    fp.write("3 0x00C "+hex(value)+"\n")
    value = int(volth*(18*4096)/(2.5*(18+91)))
    fp.write("3 0x00E "+hex(value)+"\n")
#Digitizer windows
    for ch in range(1,5):
      TPre = hex(int(app.getEntry("C"+str(ch)+"TPre")/2))
      TPost = hex(int(app.getEntry("C"+str(ch)+"TPost")/2))
      fp.write(str(3+ch)+" "+hex(int(12+4*int(ch)))+" "+TPre+"\n")
      fp.write(str(3+ch)+" "+hex(int(14+4*int(ch)))+" "+TPost+"\n")
# Channel Property
    for ch in range(1,5):
      GaindB = app.getEntry("C"+str(ch)+"Gain")
      if GaindB < -14: GaindB = -14
      if GaindB > 23.5: GaindB = 23.5
      app.setEntry("C"+str(ch)+"Gain",GaindB)
      Gain = hex(int((4096*(GaindB+14)/(37.5*2.5))+0.5))
      ITim = hex(int(app.getEntry("C"+str(ch)+"Int"))<<8)
      BMin = hex(int(app.getEntry("C"+str(ch)+"BMin")))
      BMax = hex(int(app.getEntry("C"+str(ch)+"BMax")))
      fp.write(str(5+3*ch)+" "+hex(int(20+12*int(ch)))+" "+Gain+"\n")
      fp.write(str(5+3*ch)+" "+hex(int(22+12*int(ch)))+" "+ITim+"\n")
      fp.write(str(6+3*ch)+" "+hex(int(24+12*int(ch)))+" "+BMax+"\n")
      fp.write(str(6+3*ch)+" "+hex(int(26+12*int(ch)))+" "+BMin+"\n")
# Channel Trigger
    for ch in range(1,5):
      TrigT1 = hex(int(app.getEntry("C"+str(ch)+"TrigT1")))
      TrigT2 = hex(int(app.getEntry("C"+str(ch)+"TrigT2")))
      TrigPP = int(app.getEntry("C"+str(ch)+"TrigTper")/16)<<8
      TrigPP += int(app.getEntry("C"+str(ch)+"TrigTprev")/4)
      TrigNT = int(app.getEntry("C"+str(ch)+"TrigNcmax"))<<8
      TrigNT += int(app.getEntry("C"+str(ch)+"TrigTcmax")/4)
      TrigQN = int(app.getEntry("C"+str(ch)+"TrigQmax"))<<8
      TrigQN += int(app.getEntry("C"+str(ch)+"TrigNcmin"))
      TrigQmin = hex(int(app.getEntry("C"+str(ch)+"TrigQmin")))
      fp.write(str(17+3*ch)+" "+hex(int(68+12*int(ch)))+" "+TrigT1+"\n")
      fp.write(str(17+3*ch)+" "+hex(int(70+12*int(ch)))+" "+TrigT2+"\n")
      fp.write(str(18+3*ch)+" "+hex(int(72+12*int(ch)))+" "+hex(TrigPP)+"\n")
      fp.write(str(18+3*ch)+" "+hex(int(74+12*int(ch)))+" "+hex(TrigNT)+"\n")
      fp.write(str(19+3*ch)+" "+hex(int(76+12*int(ch)))+" "+hex(TrigQN)+"\n")
      fp.write(str(19+3*ch)+" "+hex(int(78+12*int(ch)))+" "+TrigQmin+"\n")
# Filters to be interpreted by Adaq
    fp.write("#Filters to be interpreted by Adaq\n")
    for flt in range(1,5):
      for ch in range(1,5):
        fp.write(str(12+4*ch+16*flt)+" "+hex(48+16*int(ch)+64*int(flt))+" ")
        fp.write(str(app.getEntry("C"+str(ch)+"F"+str(flt)+"M"))+" ")
        fp.write(str(app.getEntry("C"+str(ch)+"F"+str(flt)+"W"))+"\n")
    fp.close()
  else:
    print("Oops")

# create a GUI variable called app
app = gui("Login Window", "500x200")
app.setBg("black")
app.setFg("yellow")
app.setFont(12)
# add & configure widgets - widgets get a name, to help referencing them later
app.addLabel("title", "Adaq gui")
#app.setLabelBg("title", "red")

# link the buttons to the function called press
app.addButtons(["Initialize", "Start run","Stop run"], press)
app.addButtons(["Configure CDAQ","Configure DU","Exit gui"], press)
app.addLabelEntry("Username")
app.addLabelSecretEntry("Password")
#
app.startSubWindow("DAQ Configuration", modal=True)
app.setFg("black")
app.setBg("orange")
app.setSize(800, 300)
row = app.getRow()
app.addButton("Read DAQ Configuration file",confbut,row,0)
app.addEntry("conffile",row,1,2)
app.setEntry("conffile","/Users/timmer/Work/GRAND/grand-daq-master/conf/Adaq.conf")
app.setEntryWidth("conffile",len(app.getEntry("conffile")))
app.addButton("Write DAQ Configuration file",confbut)
app.addLabelNumericEntry("Next Run")
app.addLabelOptionBox("Run Mode",["- Mode -","Physics","Test"])
app.setOptionBox("Run Mode","Physics",True)
app.addLabelNumericEntry("File Size")
app.addLabelNumericEntry("Random")
app.setSticky("w")
app.addLabelEntry("Data Folder",colspan=2)
app.setSticky("")
app.addTickOptionBox("Stations","")
row = app.getRow();
app.addLabel("NS","Station",row,0)
app.addLabelEntry("IP adress",row,1)
app.addLabelEntry("Port",row,2)
app.addButton("Add Station",confbut,row,3)
app.addButton("Remove Station",confbut,row,4)
app.stopSubWindow()

#
app.startSubWindow("DigitalModule", modal=True)
app.setFg("black")
app.setBg("LightBlue")
app.setSize(1100, 750)
row = 0
app.addButton("Read DU Configuration file",DUfile,row,0)
app.addEntry("DUconffile",row,1,2)
app.setEntry("DUconffile","/Users/timmer/Work/GRAND/grand-daq-master/conf/DU.conf")
app.setEntryWidth("DUconffile",len(app.getEntry("DUconffile")))
app.addNamedButton("Verify","VerifyCP",VerifyRanges,row,3)
app.addNamedButton("Exit","DigitalModule",app.hideSubWindow,row,4)
app.addButton("Write DU Configuration file",DUfile)
row = app.getRow()
app.addLabel("Bat","Battery Voltages (off, on) V")
app.addNumericEntry("BatLow",row,1)
app.setEntry("BatLow",9.0)
app.addNumericEntry("BatHigh",row,2)
app.setEntry("BatHigh",12.5)
row = row+1
app.addLabel("DU","Detector Unit")
app.addTickOptionBox("Configuration",["Auto reboot","Filter 1","Filter 2","Filter 3",
  "Filter 4","Fake ADC","1PPS","Enable DAQ"],row,1)
app.setOptionBox("Configuration","Auto reboot",True)
app.setOptionBox("Configuration","1PPS",True)
app.setOptionBox("Configuration","Enable DAQ",True)
app.addTickOptionBox("Trigger",["Channel 1","Channel 2","Channel 3","Channel 4",
  "Ch 1 AND Ch 2","(not Ch 1) AND Ch 2","Ch 1 AND Ch 2, Ch2>Ch1",
  "Ch 3 AND Ch 4","20 Hz","10 sec","Internal"],row,2)
app.setOptionBox("Trigger","Channel 1",True)
app.setOptionBox("Trigger","Channel 2",True)
app.setOptionBox("Trigger","10 sec",True)
row = row+1
app.addLabel("TP","Internal Trigger Rate [1000000, 124] Hz",row,0)
app.addNumericEntry("Test Pulse",row,1)
app.setEntry("Test Pulse",0)
row = row+1
app.addLabel("To","Trigger Overlap Time (ns)",row,0)
app.addNumericEntry("Tover",row,1)
app.setEntry("Tover",64)
row = row+1
app.addHorizontalSeparator(row,0,4,colour="red")
row = row+1
row_Input,row_Pre,row_Post,row_Gain,row_Int,row_Min,row_Max=1,2,3,4,5,6,7
trig_T1,trig_T2,trig_Tprev,trig_Tper,trig_Tcmax,trig_Ncmin,trig_Ncmax,trig_Qmin,trig_Qmax=8,9,10,11,12,13,14,15,16
filt1=17
app.addLabel("Inp","Input",row+row_Input,0)
app.addLabel("Tpre","Pre Trigger (ns)",row+row_Pre,0)
app.addLabel("Tpost","Post Trigger (ns)",row+row_Post,0)
app.addLabel("Gain","Additional Gain [-14, 23.5] (dB)",row+row_Gain,0)
app.addLabel("Tint","Integration time",row+row_Int,0)
app.addLabel("Bmin","Min. Baseline (ADC)",row+row_Min,0)
app.addLabel("Bmax","Max. Baseline (ADC)",row+row_Max,0)
app.addLabel("TrigT1","Signal Threshold (ADC)",row+trig_T1,0)
app.addLabel("TrigT2","Noise Threshold (ADC)",row+trig_T2,0)
app.addLabel("TrigTper","Time after Sig threshold (ns)",row+trig_Tper,0)
app.addLabel("TrigTrev","Quiet Time before Sig threshold (ns)",row+trig_Tprev,0)
app.addLabel("TrigTcmax","Max Time between threshold crossings (ns)",row+trig_Tcmax,0)
app.addLabel("TrigNcmin","Min number of threshold crossings",row+trig_Ncmin,0)
app.addLabel("TrigNcmax","Max number of threshold crossings",row+trig_Ncmax,0)
app.addLabel("TrigQmin","Min charge",row+trig_Qmin,0)
app.addLabel("TrigQmax","Max charge",row+trig_Qmax,0)
for flt in range(1,5):
  app.addLabel("Filt"+str(flt),"IIR Notch filter"+str(flt),row+filt1+3*flt,0)
  app.addLabel("F"+str(flt)+"Mean","\t Mean (MHz)",row+filt1+1+3*flt,0)
  app.addLabel("F"+str(flt)+"Width","\t Width 1=small, 0=infinite",row+filt1+2+3*flt,0)
for ch in range(1,5):
  app.addLabel("C"+str(ch),"Channel "+str(ch),row,ch)
  app.addOptionBox("Channel "+str(ch),
  ["Off","ADC 1","ADC 2", "ADC 3", "ADC 4", "ADC Filtered 1","ADC Filtered 2","ADC Filtered 3","ADC Filtered 4"],row+row_Input,ch)
  app.setOptionBox("Channel "+str(ch),"ADC "+str(ch))
  app.addNumericEntry("C"+str(ch)+"TPre",row+row_Pre,ch)
  app.setEntry("C"+str(ch)+"TPre",1024)
  app.addNumericEntry("C"+str(ch)+"TPost",row+row_Post,ch)
  app.setEntry("C"+str(ch)+"TPost",960)
  app.addNumericEntry("C"+str(ch)+"Gain",row+row_Gain,ch)
  app.setEntry("C"+str(ch)+"Gain",0)
  app.addNumericEntry("C"+str(ch)+"Int",row+row_Int,ch)
  app.setEntry("C"+str(ch)+"Int",5)
  app.addNumericEntry("C"+str(ch)+"BMin",row+row_Min,ch)
  app.setEntry("C"+str(ch)+"BMin",6144)
  app.addNumericEntry("C"+str(ch)+"BMax",row+row_Max,ch)
  app.setEntry("C"+str(ch)+"BMax",10240)
  app.addNumericEntry("C"+str(ch)+"TrigT1",row+trig_T1,ch)
  app.setEntry("C"+str(ch)+"TrigT1",100)
  app.addNumericEntry("C"+str(ch)+"TrigT2",row+trig_T2,ch)
  app.setEntry("C"+str(ch)+"TrigT2",50)
  app.addNumericEntry("C"+str(ch)+"TrigTper",row+trig_Tper,ch)
  app.setEntry("C"+str(ch)+"TrigTper",512)
  app.addNumericEntry("C"+str(ch)+"TrigTprev",row+trig_Tprev,ch)
  app.setEntry("C"+str(ch)+"TrigTprev",512)
  app.addNumericEntry("C"+str(ch)+"TrigTcmax",row+trig_Tcmax,ch)
  app.setEntry("C"+str(ch)+"TrigTcmax",20)
  app.addNumericEntry("C"+str(ch)+"TrigNcmin",row+trig_Ncmin,ch)
  app.setEntry("C"+str(ch)+"TrigNcmin",0)
  app.addNumericEntry("C"+str(ch)+"TrigNcmax",row+trig_Ncmax,ch)
  app.setEntry("C"+str(ch)+"TrigNcmax",10)
  app.addNumericEntry("C"+str(ch)+"TrigQmin",row+trig_Qmin,ch)
  app.setEntry("C"+str(ch)+"TrigQmin",0)
  app.addNumericEntry("C"+str(ch)+"TrigQmax",row+trig_Qmax,ch)
  app.setEntry("C"+str(ch)+"TrigQmax",255)
  for flt in range(1,5):
    app.addNumericEntry("C"+str(ch)+"F"+str(flt)+"M",row+filt1+1+3*flt,ch)
    app.setEntry("C"+str(ch)+"F"+str(flt)+"M",85+5*flt)
    app.addNumericEntry("C"+str(ch)+"F"+str(flt)+"W",row+filt1+2+3*flt,ch)
    app.setEntry("C"+str(ch)+"F"+str(flt)+"W",0.999)
row = row+34
app.stopSubWindow()
#
app.go()
