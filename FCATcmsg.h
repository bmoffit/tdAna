#ifndef __FCATCMSG__
#define __FCATCMSG__

#include "TROOT.h"
#include "cMsg.hxx"

using namespace std;
using namespace cmsg;

class FCATcmsg
{
 private:
  cMsgMessage msg;
  cMsg *cMsgSys;
  string UDL;
  string myName, myDescription;
  void *handle;//!
  cMsgCallback *callb;  /**<Callback dispatcher dispatches messages to member function.*/
  TString crate_sn;//!
  TString slot_sn[22];//!
  UShort_t *adc_payload;//!
  UInt_t *slotpayload_channelmask;//!
  
 public:
  FCATcmsg(string);
  virtual ~FCATcmsg() {Close();};
  int Init(string subject, string type);
  void SendMsg();
  void SendAnalysisDone(int, int);
  void SendStatus(TString status);
  void SendReport(TString report);
  Int_t GetSerialNumbers();
  Int_t GetPayloadConfig();
  TString GetCrateSerialNumber();
  TString GetSlotSerialNumber(int islot) {return slot_sn[islot];};
  UShort_t GetADCPayload(int isample) {return adc_payload[isample];};
  UInt_t GetPayloadIntegral();
  UInt_t GetSlotPayloadMask(int islot) {return slotpayload_channelmask[islot];};
  void SendGetMsg(string);
  void Attach();
  void Detach();
  void Disconnect();
  void Close();

  void callback(cMsgMessage *msg, void* userObject) {
      cout << "dump" << endl;
      cout << "Subject is:" << msg->getSubject() << endl;
      cout << "Type is:" << msg->getType() << endl;
      cout << "Text is:" << endl << msg->toString() << endl;
    }


  ClassDef(FCATcmsg,1)
};
#endif
