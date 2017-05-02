//  Class tdAnalysis
//
//  Bryan Moffit, April 2017
//  Analyzing data from TD fiber fifo
//

#include <byteswap.h>
#include "tdAnalysis.h"
#include "evioUtil.hxx"
#include "evioBankIndex.hxx"
//#define PRINTOUT 
#define DEBUG

using namespace evio;

tdAnalysis::tdAnalysis(int deb):
debug(deb),
fRunNumber(0),
fEventNumber(0),
fEvLength(0),
fPrevEvLength(0)
{
#ifdef DEBUG
  cout << "tdAnalysis::tdAnalysis()" << endl;
#endif
  TI = new ti_block_data[NROCS];
  TD  = new td_block_data;

  aTrigger = new Trigger[MAXBLOCKLEVEL];

  for (int itrig = 0; itrig < MAXBLOCKLEVEL; itrig++)
    {
      for (int iroc = 0; iroc < NROCS; iroc++)
	{
	}
    }

  currentTrigger = aTrigger[0];
  memset(&rocnames, 0, NROCS * sizeof(UInt_t));
  rocnames[0] = 1;
  initialized = 0;
  rocIndexFromNumber(0);
};

tdAnalysis::~tdAnalysis()
{
};

void
tdAnalysis::Init(const int *buffer, int skip = 0)
{
  // Set up ROOT.  Define output file.  Define the Output Tree.
  // This must be done at beginning of analysis.
#ifdef DEBUG
  cout << "tdAnalysis::Init()" << endl;
#endif
  if (skip)
    {
    }


  // First event should be the prestart event.  
  //  Get the runnumber from it.
  Load((const uint32_t *) buffer);

  if (IsPrestartEvent())
    fRunNumber = buffer[3];
  // FIXME: Use prestart event to get some F1 configuration

  TString fRootFileName = "ROOTfiles/fcat_";
  fRootFileName += fRunNumber;
  fRootFileName += ".root";

  cout << "ROOT Output Filename: " << fRootFileName << endl;

  evFile = new TFile(fRootFileName, "RECREATE", "FCAT ROOTfile");

  evTree = new TTree("R", "event tree");

  // Event Number Branch
  evTree->Branch("ev_num", &fEventNumber, "ev_num/I");

  for (UInt_t iroc = 0; iroc < NROCS; iroc++)
    {
      if (rocIndexFromNumber(rocnames[iroc]) != -1)
	{
	  evTree->Branch(Form("tid%d", rocnames[iroc]), &TI[iroc]);
	  evTree->Branch("td", &TD);

	}
      prev_timestamp[iroc] = 0;
      prev_eventNumber[iroc] = 0;
    }

#ifdef DOBTREE
  bTree = new TTree("B", "block tree");

  bTree->Branch("ev_num", &fEventNumber, "ev_num/I");
  bTree->Branch("Trigger", &currentTrigger);
#endif // DOBTREE
  
  initialized = 1;
};

void
tdAnalysis::Process(const int *buffer)
{
  // Process the event buffer.  Must be called for each event.
  // #define PRINTOUT
#ifdef PRINTOUT
  PrintBuffer(buffer);
#else
  Load((const uint32_t *) buffer);

  if (bankmask)
    Decode();

  if (!initialized)
    {
      cout << "WARNING: not initalized" << endl;
      return;

    }
#endif
  return;
  
  // Hack to make the warning: unused variable from evioUtils.hxx go away.
  cout << DataTypeNames[0] << endl;
};

void
tdAnalysis::Load(const uint32_t * buffer)
{
  bankmask = 0;

  // Coda event tag
  fEvLength = buffer[0] + 1;
  fCodaEvTag = (buffer[1] & 0xFFFF0000) >> 16;

  for (int ibank = 0; ibank < 10; ibank++)
    mBank[ibank] = NULL;

#define DEBUGLOAD
#ifdef DEBUGLOAD
  printf("buffer[0] = 0x%08x\n", buffer[0]);
  printf("buffer[1] = 0x%08x\n", buffer[1]);
  printf("         : fCodaEvTag = 0x%04x   fEvLength = %d\n",
	 fCodaEvTag, fEvLength);
#endif // DEBUGLOAD

  evioDOMTree event(buffer);
  evioDOMNodeListP fullList = event.getNodeList();

  for (std::list < evioDOMNodeP >::const_iterator it = fullList->begin();
       it != fullList->end(); ++it)
    {
      uint16_t tag = (*it)->tag;
      uint8_t num = (*it)->num;
      int content_type = (*it)->getContentType();

#ifdef DEBUGLOAD
      printf("type = 0x%x   tag = 0x%x  num = %d\n", content_type, tag, num);
#endif // DEBUGLOAD

      if ((tag >= 0xFFD0) && (tag <= 0xFFDF))
	{
	  /* CODA Control Event */
#ifdef DEBUGLOAD
	  switch (tag)
	    {
	    case 0xFFD0:
	      printf("\n********** SYNC Event **********\n");
	      break;

	    case 0xFFD1:
	      printf("\n********** PRESTART Event **********\n");
	      break;

	    case 0xFFD2:
	      printf("\n********** GO Event **********\n");
	      break;

	    case 0xFFD3:
	      printf("\n********** PAUSE Event **********\n");
	      break;

	    case 0xFFD4:
	      printf("\n********** END Event **********\n");
	      break;

	    default:
	      printf("  Unknown Control Event Tag\n");
	    }

#endif // DEBUGLOAD
	}

      else if ((tag >= 0xFF50) && (tag <= 0xFF8F))
	{
	  /* Physics Event */

#ifdef DEBUGLOAD
	  printf("\n********** PHYSICS Event **********\n");
#endif // DEBUGLOAD

	  // loop over physics event banks here.
	  it++;
	  tag = (*it)->tag;
	  num = (*it)->num;
	  content_type = (*it)->getContentType();

#ifdef DEBUGLOAD
	  printf("    tag = 0x%x  num = 0x%x   content_type = 0x%x\n",
		 tag, num, content_type);
#endif
	  
	  if ((tag >= 0xFF10) && (tag <= 0xFF4F))
	    {
	      /* Trigger Bank */

#ifdef DEBUGLOAD
	      printf("--- Trigger Bank ---\n");
#endif // DEBUGLOAD

	      evioDOMNodeListP trigList = (*it)->getChildren();
	      for (std::list < evioDOMNodeP >::const_iterator itChild =
		     trigList->begin(); itChild != trigList->end(); ++itChild)
		{
		  int child_type = (*itChild)->getContentType();
#ifdef DEBUGLOAD
		  printf("   child_type = 0x%x\n", child_type);
#endif // DEBUGLOAD
		  switch (child_type)
		    {
		    case 0xa:	// Event Number/Timestamp
		      {
			const vector < uint64_t > *vec =
			  (*itChild)->getVector < uint64_t > ();
			fEventNumber = (Int_t) ((*vec)[0]);
			fTimeStamp = (Int_t) ((*vec)[1]);
			break;
		      }

		    case 0x5:	// Event types
		      {
			const vector < uint16_t > *vec =
			  (*itChild)->getVector < uint16_t > ();
			fEventType = (*vec)[0];
			break;
		      }

		    case 0x1:	// Extra TI data
		      break;

		    default:
		      printf("%s: ERROR: Unknown data type %d for tag=0x%04x\n",
			     __func__, child_type, tag);
		    }
		  it++;
		}
#ifdef DEBUGLOAD
	      printf
		(" fEventNumber = 0x%x   fTimeStamp = 0x%x  fEventType = 0x%x\n",
		 fEventNumber, fTimeStamp, fEventType);
#endif // DEBUGLOAD

	      int iroc = 0, iev = 0;
	      aTrigger[iev].Clear();

	      aTrigger[iev].fTI[iroc].eventNumber = fEventNumber;
	      aTrigger[iev].fTI[iroc].timestamp = fTimeStamp;

	      aTrigger[iev].fTI[iroc].prev_eventNumber = prev_eventNumber[iroc];
	      aTrigger[iev].fTI[iroc].prev_timestamp = prev_timestamp[iroc];

	      if (aTrigger[iev].fTI[iroc].timestamp <
		  aTrigger[iev].fTI[iroc].prev_timestamp)
		aTrigger[iev].fTI[iroc].rollover++;

	      UInt_t trigdiff;

	      if (fTimeStamp < prev_timestamp[iroc])
		trigdiff = ((1ULL) << 32) + prev_timestamp[iroc] - fTimeStamp;
	      else
		trigdiff = fTimeStamp - prev_timestamp[iroc];

	      trigdiff /= (fEventNumber - prev_eventNumber[iroc]);

	      prev_eventNumber[iroc] = aTrigger[iev].fTI[iroc].eventNumber;
	      prev_timestamp[iroc] = aTrigger[iev].fTI[iroc].timestamp;

	    }

	  if (content_type == 0x10)	// ROC 0
	    {
#ifdef DEBUGLOAD
	      printf(" tag = 0x%x num = %d\n",
		     tag, num);
#endif // DEBUGLOAD
	      evioDOMNodeListP rocList = (*it)->getChildren();
	      for (std::list < evioDOMNodeP >::const_iterator itRoc =
		     rocList->begin(); itRoc != rocList->end(); ++itRoc)
		{
		  int bank_type = (*itRoc)->getContentType();
		  fBlocklevel = (*itRoc)->num;
		  int bank_tag = (*itRoc)->tag;
#ifdef DEBUGLOAD
		  printf("   bank_tag = 0x%x   type = %d   blocklevel = %d\n",
			 bank_tag, bank_type, fBlocklevel);
#endif // DEBUGLOAD
		  switch (bank_tag)
		    {
		    case 0x1:	// misc
		    case 0x3:	// fADC250
		    case 0x5:	// stats
		    case 0x6:	// CTP
		      {
			bankmask |= (1 << bank_tag);
			break;
		      }
		    case 0x0a:     // TD
		      {
			bankmask |= (1 << bank_tag);
			break;
		      }
		    default:
		      printf("%s: ERROR: Unknown bank 0x%04x\n",
			     __func__, bank_tag);
		    }
		  it++;
		}
	    }
      
	}

    }


  // if (IsPhysicsEvent())
  if (bankmask)
    {
      evioBankIndex bi(buffer);
      evioDictEntry tn;
      // const uint32_t *evBuf32;
      int len;

      // TD Bank
      tn = evioDictEntry(0xa, fBlocklevel);
      if (bi.tagNumExists(tn))
	mBank[0xa] =
	  (moduleBank *) (bi.getData < uint32_t > (tn, &len) - 2);
#ifdef OLDBANKS
      // FADC bank
      tn = evioDictEntry(3, fBlocklevel);
      if (bi.tagNumExists(tn))
	mBank[3] =
	  (moduleBank *) (bi.getData < uint32_t > (tn, &len) - 2);

      // CTP bank
      tn = evioDictEntry(6, fBlocklevel);
      if (bi.tagNumExists(tn))
	mBank[6] =
	  (moduleBank *) (bi.getData < uint32_t > (tn, &len) - 2);

      // Setup bank
      tn = evioDictEntry(5, 0);
      if (bi.tagNumExists(tn))
	mBank[5] =
	  (moduleBank *) (bi.getData < uint32_t > (tn, &len) - 2);
#endif
#ifdef DEBUGLOAD
      int ibank;
      for (int ibank = 0; ibank < 8; ibank++)
	{
	  if (mBank[ibank])
	    {
	      printf("%s: bank %d  length = %d\n",
		     __func__, ibank, mBank[ibank]->length);
	    }
	}
#endif // DEBUGLOAD

    }

  fPrevEvLength = fEvLength;

}

void
tdAnalysis::End()
{
  // End of analysis.  
#ifdef DEBUG
  cout << "tdAnalysis::End()" << endl;
#endif

  evFile->Write();
  evFile->Close();

};

void
tdAnalysis::Decode()
{
  Bool_t foundData = kFALSE;


  aTrigger[0].Clear();

  if (mBank[0xa])			// TD Bank
    {
      DecodeTD(mBank[0xa]);
      foundData = kTRUE;
    }

  if (foundData)
    {
      evTree->Fill();
#ifdef DOBTREE
      for(UInt_t iev=0; iev<blocklevel; iev++)
	{
	  currentTrigger = aTrigger[iev];
	  bTree->Fill();
	  //      bTree->ResetBranchAddresses();
	}
#endif
      foundData = kFALSE;
    }
  //   else
  //     {
  //       printf("%6d: No data found\n",fEventNumber);
  //     }
}

void
tdAnalysis::DecodeTD(moduleBank *tdbank)
{
  UInt_t iword=0, data=0;
  td_fiber_word_t tdword;
  int blkrec = 0;
  
  TD->Clear();
  
  while (iword < (tdbank->length-1))
    {
      data = tdbank->data[iword];
#define BYTESWAPTD
#ifdef BYTESWAPTD
      data = bswap_32(data);
#endif

      tdword.raw = data;
#ifdef DEBUGTD
      printf("%8d: 0x%08x  timestamp = 0x%04x\n",
	     iword, data, tdword.bf.timestamp);
#endif // DEBUGTD

      /* Bump counters, according to set bits */
      if(tdword.bf.busy)
	{
	  TD->busy_cnt++;
	}

      if(tdword.bf.trg_ack)
	{
	  TD->trg_ack_cnt++;
	}
      
      if(tdword.bf.sync_reset_req)
	{
	  TD->sync_reset_req_cnt++;
	}

      if(tdword.bf.not_sync_reset_req)
	{
	  TD->not_sync_reset_req_cnt++;
	}

      if(tdword.bf.busy2)
	{
	  TD->busy2_cnt++;
	}

      if(tdword.bf.trig1_ack)
	{
	  TD->trig1_ack_cnt++;
	}

      if(tdword.bf.trig2_ack)
	{
	  TD->trig2_ack_cnt++;
	}

      if(tdword.bf.block_received)
	{
	  blkrec = 1;
	  TD->block_received_cnt++;
	}

      if(tdword.bf.readout_ack)
	{
	  TD->readout_ack_cnt++;
	}

#ifdef DEBUGTD
      printf("%8d: 0x%08x  timestamp = 0x%04x  %s\n",
	     iword, data, tdword.bf.timestamp, blkrec?"*":" ");
#endif
      blkrec = 0;
      iword++;
    }

#ifdef DEBUGTD
  printf("Counts\n");
  printf("********************************************************************************\n");
  printf("Busy   TrgAck SynReq Busy2  Trig1  Trig2  BlkRec ROAck \n");
  printf("%4d   ", TD->busy_cnt);
  printf("%4d   ", TD->trg_ack_cnt);
  printf("%4d   ", TD->sync_reset_req_cnt);
  printf("%4d   ", TD->busy2_cnt);
  printf("%4d   ", TD->trig1_ack_cnt);
  printf("%4d   ", TD->trig2_ack_cnt);
  printf("%4d   ", TD->block_received_cnt);
  printf("%4d   ", TD->readout_ack_cnt);
  printf("\n");
#endif

}


void
tdAnalysis::PrintBuffer(const uint32_t * buffer)
{
  // Print the event buffer for diagnostics.
  int len = buffer[0] + 1;
  int evtype = buffer[1] >> 16;
  int evnum = buffer[4];
  cout << "\n\n Event number " << dec << evnum;
  cout << " length " << len << " type " << evtype << endl;
  int ipt = 0;
  for (int j = 0; j < (len / 5); j++)
    {
      cout << dec << "\n evbuffer[" << ipt << "] = ";
      for (int k = j; k < j + 5; k++)
	{
	  printf("0x%08x ", buffer[ipt++]);
	  //      cout << hex << buffer[ipt++] << " ";
	}
      cout << endl;
    }
  if (ipt < len)
    {
      cout << dec << "\n evbuffer[" << ipt << "] = ";
      for (int k = ipt; k < len; k++)
	{
	  printf("0x%08x ", buffer[ipt++]);
	  //      cout << hex << buffer[ipt++] << " ";
	}
      cout << endl;
    }

#ifdef NOTUSED
  int idata;
  printf("ROC1 Data\n");
  for (int i = 0; i < fRoc1Length; i++)
    {
      idata = iRoc1Pos + i;
      if ((i % 4) == 0)
	{
	  printf("\n%8d: ", i);
	}
      printf("  0x%08x", buffer[idata]);
    }
  printf("\n");
  /* ROC 1 data */
  printf("ROC6 Data\n");
  for (int i = 0; i < fRoc6Length; i++)
    {
      idata = iRoc6Pos + i;
      if ((i % 4) == 0)
	{
	  printf("\n%8d: ", i);
	}
      printf("  0x%08x", buffer[idata]);
    }
  printf("\n");
#endif
};

void
tdAnalysis::PrintData()
{
  //   cout << "Event " << dec << fEventNumber << endl;
  //   int buffpos = 0;
  //   for (int i = 0; i < (MAXCHAN/8); i++) {
  //     cout << "ADC[" << dec << buffpos << "] = ";
  //     for (int j = 0; j<8; j++) { 
  //       cout << setw(4) << hex << adc[buffpos++] << "  ";
  //     }
  //     cout << endl;
  //   }
  //   if (buffpos < MAXCHAN) {
  //     cout << "ADC[" << dec << buffpos << "] = ";
  //     for (int j = buffpos; j < MAXCHAN; j++) {
  //       cout << setw(4) << hex << adc[buffpos++] << "  ";
  //     }
  //     cout << endl;
  //   }
  //   buffpos = 0;
  //   for (int i = 0; i < (MAXCHAN/8); i++) {
  //     cout << "SCA[" << dec << buffpos << "] = ";
  //     for (int j = 0; j<8; j++) { 
  //       cout << setw(4) << hex << sca[buffpos++] << "  ";
  //     }
  //     cout << endl;
  //   }
  //   if (buffpos < MAXCHAN) {
  //     cout << "SCA[" << dec << buffpos << "] = ";
  //     for (int j = buffpos; j < MAXCHAN; j++) {
  //       cout << setw(4) << hex << sca[buffpos++] << "  ";
  //     }
  //     cout << endl;
  //   }
  //   buffpos = 0;
  //   for (int i = 0; i < (MAXCHAN/8); i++) {
  //     cout << "TIMER[" << dec << buffpos << "] = ";
  //     for (int j = 0; j<8; j++) { 
  //       cout << setw(4) << hex << timer[buffpos++] << "  ";
  //     }
  //     cout << endl;
  //   }
  //   if (buffpos < MAXCHAN) {
  //     cout << "TIMER[" << dec << buffpos << "] = ";
  //     for (int j = buffpos; j < MAXCHAN; j++) {
  //       cout << setw(4) << hex << timer[buffpos++] << "  ";
  //     }
  //     cout << endl;
  //   }
  //   cout << "TIRDAT = " << setw(4) << hex << tirdata;
  //   cout << endl << endl;
}

Bool_t
tdAnalysis::IsPrestartEvent() const 
{
  Bool_t rval = kFALSE;

  if (fCodaEvTag == 0xFFD1)
    {
      rval = kTRUE;
    }

  return rval;
}

Bool_t
tdAnalysis::IsPhysicsEvent() const 
{
  Bool_t rval = kFALSE;

  if ((fCodaEvTag >= 0xFF50) && (fCodaEvTag <= 0xFF8F))
    {
      rval = kTRUE;
    }

  return rval;
}

Bool_t
tdAnalysis::IsSetupEvent() const 
{
  // Leave this in here for now... but it's always false.
  return kFALSE;

  return (fEvType == 13);
}



Int_t
tdAnalysis::rocIndexFromNumber(int rocNumber)
{
  static int initd = 0;
  static int rocIndex[NROCS];

  //  rocnames[0] = 1
  //  rocnames[1] = 5

  //  rocIndex[1] = 0
  //  rocIndex[5] = 1

  if (initd == 0)
    {
      // Go through and make the reverse map
      for (UInt_t index = 0; index < NROCS; index++)
	{
	  rocIndex[index] = -1;
	}
      for (UInt_t index = 0; index < NROCS; index++)
	{
	  if (rocnames[index])
	    rocIndex[rocnames[index]] = index;
	}
      initd = 1;
    }

  return rocIndex[rocNumber];
}

