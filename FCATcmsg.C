#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include "FCATcmsg.h"
#include <signal.h>
#include <string>

ClassImp(FCATcmsg);



FCATcmsg::FCATcmsg(string name) 
  : callb(0)
{
  callb = new cMsgDispatcher<FCATcmsg>(this,&FCATcmsg::callback);

  adc_payload = new UShort_t[32];
  slotpayload_channelmask = new UInt_t[22];

  crate_sn = "none";

  UDL = "cMsg://multicast/cMsg/fcat-fa250/FCAT?regime=low&cmsgpassword=FCAT";
  myName=name;
  myDescription="An Analysis Machine";

  cMsgSys = new cMsg(UDL, myName, myDescription);
  try 
    {
      cMsgSys->connect();
    } 
  catch (cMsgException e) 
    {
      cout << "punt" << endl;
      cout << e.toString() << endl;
    }

  int isConnected=0;
  isConnected = cMsgSys->isConnected();
  cout << "isConnected = " << isConnected << endl;

  cout << __FUNCTION__ << ": Constructed" << endl;

}

int
FCATcmsg::Init(string subject, string type)
{
  msg.setSubject(subject);
  msg.setType(type);

  return 0;
}

void
FCATcmsg::SendMsg()
{
  if(!cMsgSys->isConnected())
    return;

  int myInt = 1;
  struct timespec myTime;
  string mySubject = "SUBJECT", myType = "TY", myText = "txt";
  clock_gettime(CLOCK_REALTIME, &myTime);
  msg.setSubject(mySubject);

  
  msg.setType(myType);
  msg.setUserInt(myInt);
  msg.setUserTime(myTime);
  msg.setText(myText);
  // add payload item (array of strings)
  string name = "STR_ARRAY", name2 = "MSG";
  string strs[3] = {"one", "two", "three"};
  msg.add(myName, strs, 3);
  // add another payload item (cMsg message)
  cMsgMessage myMsg;
  myMsg.setSubject("sub");
  myMsg.setType("type");
  msg.add(name2, myMsg);

  cMsgSys->send(msg);
  cMsgSys->flush();

}

void
FCATcmsg::SendAnalysisDone(int pass, int grade)
{
  if(!cMsgSys->isConnected())
    return;

  struct timespec myTime;
  string mySubject = "SUBJECT", myType = "TY", myText = "txt";
  clock_gettime(CLOCK_REALTIME, &myTime);
  msg.setSubject("ANA");

  
  msg.setType("Status");
  if(pass==1)
    msg.setText("Analysis1 Complete");
  else
    msg.setText("Analysis2 Complete");
  msg.add("grade",grade);
  cMsgSys->send(msg);
  cMsgSys->flush();

}

void
FCATcmsg::SendReport(TString report)
{
  if(!cMsgSys->isConnected())
    return;

  msg.setSubject("ANA");
  msg.setType("Report");
  cout << "report = " << report.Data() << endl;
  msg.setText(report.Data());

  cMsgSys->send(msg);
  cMsgSys->flush();

}

void
FCATcmsg::SendStatus(TString status)
{
  if(!cMsgSys->isConnected())
    return;

  msg.setSubject("ANA");
  msg.setType("Status");

  msg.setText(status.Data());

  cMsgSys->send(msg);
  cMsgSys->flush();

}

Int_t
FCATcmsg::GetSerialNumbers()
{
  Int_t rval=0;
  struct timespec myTimeout = {5,0};

  if(!cMsgSys->isConnected())
    return -1;

  msg.setSubject("ANA");
  msg.setType("GetSerialNumbers");

  cMsgMessage *response;
  try
    {
      response = cMsgSys->sendAndGet(msg, &myTimeout);
    }
  catch (cMsgException e)
    {
      cout << __FUNCTION__ << "\n\t";
      cout << e.toString() << endl;

      return -1;
    }

  if(response->isGetResponse())
    {
#ifdef DEBUGCMSG
      string out = response->toString();
      cout << __FUNCTION__ << ": Here's what we got" << endl;
      cout << out << endl;
#endif
      string *tmpStringArray;

      tmpStringArray = response->getStringArray("SlotSerialNumbers");
      for(int islot=0; islot<22; islot++)
	slot_sn[islot] = (TString)tmpStringArray[islot];

      crate_sn = (TString)response->getString("CrateSerialNumber");
      rval=0;
    }
  else
    {
      cout << __FUNCTION__ << ": Something wrong here" << endl;
      rval = -1;
    }

  return rval;

}

Int_t
FCATcmsg::GetPayloadConfig()
{
  Int_t rval=0;
  struct timespec myTimeout = {5,0};

  if(!cMsgSys->isConnected())
    return -1;

  msg.setSubject("ANA");
  msg.setType("GetPayloadConfig");

  cMsgMessage *response;
  try
    {
      response = cMsgSys->sendAndGet(msg, &myTimeout);
    }
  catch (cMsgException e)
    {
      cout << __FUNCTION__ << ": Woop!" << endl;
      cout << e.toString() << endl;

      return -1;
    }

  if(response->isGetResponse())
    {
#ifdef DEBUGCMSG
      string out = response->toString();
      cout << __FUNCTION__ << ": Here's what we got" << endl;
      cout << out << endl;
#endif
      adc_payload = (UShort_t*)response->getUint16Array("Payload");
      slotpayload_channelmask = (UInt_t*)response->getUint32Array("ModuleChannelMask");
      rval = 0;
    }
  else
    {
      cout << __FUNCTION__ << ": Something wrong here" << endl;
      rval = -1;
    }


  return rval;
}

UInt_t
FCATcmsg::GetPayloadIntegral()
{
  UInt_t integral=0;

  for(int isample=0; isample<32; isample++)
    {
      integral += (UInt_t)adc_payload[isample];
    }

  return integral;
}

TString 
FCATcmsg::GetCrateSerialNumber()
{
  if(crate_sn)
    return crate_sn;
  else
    return "none";
}

void
FCATcmsg::SendGetMsg(string type)
{
  struct timespec myTimeout = {5,0};

  if(!cMsgSys->isConnected())
    return;

  msg.setSubject("ROC");
  msg.setType(type);

  // add payload item (array of strings)
  string name = "STR_ARRAY", name2 = "MSG";
  string strs[3] = {"one", "two", "three"};
  msg.add(myName, strs, 3);
  
  cMsgMessage *response;
  try
    {
      response = cMsgSys->sendAndGet(msg, &myTimeout);
    }
  catch (cMsgException e)
    {
      cout << __FUNCTION__ << ": Woop!" << endl;
      cout << e.toString() << endl;

      return;
    }

  if(response->isGetResponse())
    {
#ifdef DEBUGCMSG
      string out = response->toString();
      cout << __FUNCTION__ << ": Here's what we got" << endl;
      cout << out << endl;
#endif
    }
  else
    cout << __FUNCTION__ << ": Something wrong here" << endl;
}

void
FCATcmsg::Attach()
{
  handle = cMsgSys->subscribe("ROC", "STATE", callb, NULL);
  cMsgSys->start();

}

void
FCATcmsg::Detach()
{
  cMsgSys->unsubscribe(handle);
  cMsgSys->stop();
}

void
FCATcmsg::Disconnect()
{
  cMsgSys->disconnect();
}

void
FCATcmsg::Close()
{
  cMsgSys->disconnect();
  delete cMsgSys;
}

