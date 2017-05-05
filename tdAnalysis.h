#ifndef __TDANALYSIS_H__
#define __TDANALYSIS_H__
// -*- c++ -*-
//  Class tdAnalysis
//
//  Bryan Moffit, April 2017
//  Analyzing data from TD fiber fifo
//

#include <stdint.h>
#include <iostream>
#include "TROOT.h"
#include "TFile.h"
#include "TTree.h"

#define MAXCHAN 16
#define NROCS   4
#define MAXBLOCKLEVEL 256

using namespace std;

typedef struct
{
  uint32_t length;
  uint32_t header;
  uint32_t data[];
} moduleBank;

class ti_data:public TObject
{
public:
  UInt_t eventHeader;
  UInt_t eventNumber;
  UInt_t prev_eventNumber;
  Long64_t prev_timestamp;
  Long64_t timestamp;
  UInt_t triggerData;
  UInt_t extraData1;
  UInt_t extraData2;
  UInt_t tofb;
  UInt_t rollover;

  ti_data()
  {
    rollover = 0;
  }
  void Clear()
  {
    eventHeader = 0;
    eventNumber = 0;
    triggerData = 0;
    extraData1 = 0;
    extraData2 = 0;
    tofb = 0;
  }

  ClassDef(ti_data, 2);
};


class td_fiber_data:public TObject
{
  
public:

  uint16_t readout_ack;
  uint16_t block_received;
  uint16_t trig2_ack;
  uint16_t trig1_ack;
  uint16_t busy2;
  uint16_t not_sync_reset_req;
  uint16_t sync_reset_req;
  uint16_t trg_ack;
  uint16_t busy;
  uint16_t header;
  uint16_t timestamp;
  uint32_t raw;

  td_fiber_data()
    {
      raw = 0;
    }

  void SetRaw(uint32_t b)
    {
      struct td_fiber_word
      {
	uint16_t undef:3;              // bits(2:0)
	uint16_t readout_ack:1;        // 3
	uint16_t block_received:1;     // 4
	uint16_t trig2_ack:1;          // 5
	uint16_t trig1_ack:1;          // 6
	uint16_t busy2:1;              // 7
	uint16_t not_sync_reset_req:1; // 8
	uint16_t sync_reset_req:1;     // 9
	uint16_t trg_ack:1;            // 10
	uint16_t busy:1;               // 11
	uint16_t header:4;             // bits(15:12)
	uint16_t timestamp:16;         // bits(31:16)
      };
      
      union flub
      {
	struct td_fiber_word bf1;
	uint32_t rrr;
      } uni;

      uni.rrr = b;
      raw = uni.rrr;
      readout_ack = uni.bf1.readout_ack;
      block_received = uni.bf1.block_received;
      trig2_ack = uni.bf1.trig2_ack;
      trig1_ack = uni.bf1.trig1_ack;
      busy2 = uni.bf1.busy2;
      not_sync_reset_req = uni.bf1.not_sync_reset_req;
      sync_reset_req = uni.bf1.sync_reset_req;
      busy = uni.bf1.busy;
      header = uni.bf1.header;
      timestamp = uni.bf1.timestamp;
      
    }

  ClassDef(td_fiber_data, 2);
};

class td_block_data:public TObject
{
public:
  UInt_t busy_cnt;
  UInt_t trg_ack_cnt;
  UInt_t sync_reset_req_cnt;
  UInt_t not_sync_reset_req_cnt;
  UInt_t busy2_cnt;
  UInt_t trig1_ack_cnt;
  UInt_t trig2_ack_cnt;
  UInt_t block_received_cnt;
  UInt_t readout_ack_cnt;
  vector <td_fiber_data> decoded;
  
  td_block_data()
  {
    Clear();
    decoded.reserve(4 * MAXBLOCKLEVEL);
  }

  void Clear()
  {
    busy_cnt = 0;
    trg_ack_cnt = 0;
    sync_reset_req_cnt = 0;
    not_sync_reset_req_cnt = 0;
    busy2_cnt = 0;
    trig1_ack_cnt = 0;
    trig2_ack_cnt = 0;
    block_received_cnt = 0;
    readout_ack_cnt = 0;
    decoded.clear();
  }


  ClassDef(td_block_data, 2);
};

class ti_block_data:public TObject
{
public:
  UInt_t eventNumber;
  vector <UInt_t> eventType;
  vector <Long64_t> timestamp;

  ti_block_data()
  {
    eventType.reserve(MAXBLOCKLEVEL);
    timestamp.reserve(MAXBLOCKLEVEL);
    Clear();
  }
  void Clear()
  {
    eventNumber = 0;
    eventType.clear();
    timestamp.clear();
  }


  ClassDef(ti_block_data, 2);
};


class Trigger:public TObject
{
public:
  // TI Data
  ti_data fTI[NROCS];
  // TD Fiber Data (max it out at 4 * blocklevel)
  UInt_t eventNumber;
  UInt_t eventType;
  Long64_t timestamp;
  
  Trigger()
  {
    Clear();
    for (Int_t iroc = 0; iroc < NROCS; iroc++)
      {
      }
  }

  void Clear()
  {

    for (Int_t iroc = 0; iroc < NROCS; iroc++)
      {
	fTI[iroc].Clear();
      }
  }

  void Init(int blocklevel)
  {
  }

  ClassDef(Trigger, 2);
};


class tdAnalysis
{

public:

  tdAnalysis(int);
  ~tdAnalysis();
  void Init(const int *buffer, int skip);	//!
  void Process(const int *buffer);	//!
  void End();
private:

  tdAnalysis(const tdAnalysis & fn);
  tdAnalysis & operator=(const tdAnalysis & fn);
  void InitTrees();
  void Load(const uint32_t * buffer);
  void Decode();
  void DecodeTD(moduleBank *tdbank);
  void PrintBuffer(const uint32_t * buffer);	// print data buffer for diagnostics
  void PrintData();		// print data (sorted)
  Bool_t IsPrestartEvent() const;
  Bool_t IsPhysicsEvent() const;
  Bool_t IsSetupEvent() const;
  Int_t rocIndexFromNumber(int rocNumber);
  UInt_t DecodeSetup(moduleBank *setupbank);

  // Bank Info
  moduleBank *mBank[20];

  vector <UInt_t> rocnames;
  Int_t debug;
  Int_t fCodaEvTag;
  UInt_t fBlocklevel;
  Int_t fRunNumber;
  Int_t fEventNumber;
  vector <UInt_t> fEventType;
  vector <Long64_t> fTimeStamp;
  Int_t fEvLength;
  Int_t fPrevEvLength;
  Int_t fSyncFlag;
  UInt_t bankmask;

  ti_block_data *Trig;
  ti_block_data *TI;
  td_block_data *TD;
  
  TFile *evFile;		// ROOT output file
  TTree *evTree;		// Event Tree
  TTree *bTree;			// Event Tree
  Int_t initialized;
  Int_t initd_trees;
  UInt_t fEvType;
  UInt_t prev_eventNumber[NROCS];
  Long64_t prev_timestamp[NROCS];

  Trigger currentTrigger;	// current trigger to write into tree
  Trigger *aTrigger;		// Array of triggers from current event, size = blocklevel

};

#endif // __TDANALYSIS_H__
