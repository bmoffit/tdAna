//  Class FCATAnalysis
//
//  Bryan Moffit, October 2012
//  Analyzing data from the Full Crate Acceptance Test
//
#include <stdint.h>
#include "FCATAnalysis.h"
#include "evioBankIndex.hxx"
//#define PRINTOUT 
#define DEBUG

FCATAnalysis::FCATAnalysis(int deb):
  debug(deb),
  fRunNumber(0),
  fEventNumber(0),
  fEvLength(0),
  fPrevEvLength(0)
{
#ifdef DEBUG
  cout << "FCATAnalysis::FCATAnalysis()" << endl;
#endif
  TID = new tid_block_data[NROCS];
  ROC = new roc_data[NROCS];
  FADC = new fadc_block_data[21 * NROCS];
  F1TDC = new f1tdc_block_data[21 * NROCS];
  aTrigger = new Trigger[MAXBLOCKLEVEL];
  runstats = new stats;

  for (int itrig = 0; itrig < MAXBLOCKLEVEL; itrig++)
    {
      for (int iroc = 0; iroc < NROCS; iroc++)
	for (int imod = 0; imod < FADC_MAX_MODULES; imod++)
	  {
	    
	    for (int ichan = 0; ichan < FADC_MAX_CHAN; ichan++)
	      {
		aTrigger[itrig].fmod[imod].fchan[ichan].raw_data =
		  new Int_t[FADC_MAX_SAMPLES];
		aTrigger[itrig].fmod[imod].fchan[ichan].sample =
		  new Int_t[FADC_MAX_SAMPLES];

		aTrigger[itrig].fmod[imod].fchan[ichan].pulse_integral =
		  new Int_t[FADC_MAX_PULSES];
		aTrigger[itrig].fmod[imod].fchan[ichan].pulse_time =
		  new Int_t[FADC_MAX_PULSES];
	      }
	    for (int ichan = 0; ichan < F1TDC_MAX_CHAN; ichan++)
	      {
		aTrigger[itrig].f1mod[imod].fchan[ichan].time =
		  new Int_t[F1TDC_MAX_HITS];
		aTrigger[itrig].f1mod[imod].fchan[ichan].ctime =
		  new Int_t[F1TDC_MAX_HITS];
		aTrigger[itrig].f1mod[imod].fchan[ichan].trigtime =
		  new Int_t[F1TDC_MAX_HITS];
	      }
	  }
    }

  currentTrigger = aTrigger[0];
  memset(&rocnames, 0, NROCS * sizeof(UInt_t));
  rocnames[0] = 1;
  initialized = 0;
  rocIndexFromNumber(0);
  skipFADC = kFALSE;
  skipBlock = kFALSE;
};

FCATAnalysis::~FCATAnalysis()
{
};

void
FCATAnalysis::Init(const int *buffer, int skip = 0)
{
  // Set up ROOT.  Define output file.  Define the Output Tree.
  // This must be done at beginning of analysis.
#ifdef DEBUG
  cout << "FCATAnalysis::Init()" << endl;
#endif
  if (skip)
    {
      skipBlock = kTRUE;
      skipFADC = kTRUE;
    }


  // First event should be the prestart event.  
  //  Get the runnumber from it.
  Load((const uint32_t *) buffer);

  if (IsPrestartEvent())
    fRunNumber = buffer[3];
  // FIXME: Use prestart event to get some F1 configuration

  TString fRootFileName = "ROOTfiles/fcat_";
  if (skipBlock)
    fRootFileName += "noBlock_";
  else if (skipFADC)
    fRootFileName += "noFADC_";
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
	  evTree->Branch(Form("tid%d", rocnames[iroc]), &TID[iroc]);
	  if (!skipFADC)
	    {
	      for (Int_t imod = 0; imod < FADC_MAX_MODULES; imod++)
		{
		  evTree->Branch(Form("fadc%d_%d", rocnames[iroc], imod),
				 &FADC[imod + (iroc * 16)]);
		}
	    }
	  evTree->Branch(Form("roc%d", rocnames[iroc]), &ROC[iroc]);

	}
      prev_timestamp[iroc] = 0;
      prev_eventNumber[iroc] = 0;
    }

  bTree = new TTree("B", "block tree");
  if (!skipBlock)
    {
      bTree->Branch("ev_num", &fEventNumber, "ev_num/I");
      bTree->Branch("Trigger", &currentTrigger);
      for (int imod = 0; imod < FADC_MAX_MODULES; imod++)
	{
	  if ((imod < 3) || (imod == 11) || (imod == 12) || (imod > 20))
	    continue;
	  TString bname = Form("fadc%d", imod);
	  bTree->Branch(bname, &currentTrigger.fmod[imod]);
	}
      
      trigger_period = new TH1I("trigger_period",
			    "Trigger Period",
			    10000, 0, 400000);
    }

  for (int imod = 0; imod < FADC_MAX_MODULES; imod++)
    {
      mod_hit[imod]
	= new TH1I(Form("mod_hit_%d", imod),
		   Form("Slot %d Hits", imod), 
		   22, 0, 22);
      timestamp_diff[imod]
	= new TH1I(Form("timestamp_diff_%d", imod),
		   Form("Slot %d TimeStamp Difference", imod),
		   30, 0, 30);
      
      if ((imod < 3) || (imod == 11) || (imod == 12) || (imod > 20))
	{
	  delete mod_hit[imod];
	  delete timestamp_diff[imod];
	}

      for (int ichan = 0; ichan < FADC_MAX_CHAN; ichan++)
	{
	  channel_hit[imod][ichan]
	    = new TH1I(Form("channel_hit_%d_%d", imod, ichan),
		       Form("Slot %d Channel %d hits", imod, ichan),
		       16, 0, 16);
	  raw_data[imod][ichan]
	    = new TH1I(Form("raw_data_%d_%d", imod, ichan),
		       Form("Slot %d Channel %d raw data", imod, ichan),
		       200, 90, 400);
	  pulse_time[imod][ichan]
	    = new TH1I(Form("pulse_time_%d_%d", imod, ichan),
		       Form("Slot %d Channel %d pulse time", imod, ichan),
		       100, 128, 1280);
	  pulse_integral[imod][ichan]
	    = new TH1I(Form("pulse_integral_%d_%d", imod, ichan),
		       Form("Slot %d Channel %d pulse integral", imod, ichan),
		       100, 800, 950);

	  if ((imod < 3) || (imod == 11) || (imod == 12) || (imod > 20))
	    {
	      delete channel_hit[imod][ichan];
	      delete raw_data[imod][ichan];
	      delete pulse_time[imod][ichan];
	      delete pulse_integral[imod][ichan];
	    }
	}
    }
      
  initialized = 1;
};

void
FCATAnalysis::Process(const int *buffer)
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
};

void
FCATAnalysis::Load(const uint32_t * buffer)
{
  bankmask = 0;

  // Coda event tag
  fEvLength = buffer[0] + 1;
  fCodaEvTag = (buffer[1] & 0xFFFF0000) >> 16;

  for(int ibank = 0; ibank < 10; ibank++)
    mBank[ibank] = NULL;
  
// #define DEBUGLOAD
#ifdef DEBUGLOAD
  printf("buffer[0] = 0x%08x\n", buffer[0]);
  printf("buffer[1] = 0x%08x\n", buffer[1]);
  printf("         : fCodaEvTag = 0x%04x   fEvLength = %d\n",
	 fCodaEvTag, fEvLength);
#endif // DEBUGLOAD
  
  evioDOMTree event(buffer);
  evioDOMNodeListP fullList = event.getNodeList();
  
  for(std::list<evioDOMNodeP>::const_iterator it = fullList->begin();
      it != fullList->end(); ++it)
    {
      uint16_t tag = (*it)->tag;
      // uint8_t num = (*it)->num;
      int content_type = (*it)->getContentType();

#ifdef DEBUGLOAD
      printf("type = 0x%x   tag = 0x%x  num = %d\n",
	     content_type, tag, num);
#endif // DEBUGLOAD
    
      if((tag >= 0xFFD0) && (tag <= 0xFFDF))
	{
	  /* CODA Control Event */
#ifdef DEBUGLOAD
	  switch(tag)
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

      if((tag >= 0xFF50) && (tag <= 0xFF8F))
	{
	  /* Physics Event */
#ifdef DEBUGLOAD
	  printf("\n********** PHYSICS Event **********\n");
#endif // DEBUGLOAD
	}

      if((tag >= 0xFF10) && (tag <= 0xFF4F))
	{
	  runstats->nEntries++;
	  /* Trigger Bank */
	  evioDOMNodeListP trigList = (*it)->getChildren();
	  for(std::list<evioDOMNodeP>::const_iterator itChild = trigList->begin();
	      itChild != trigList->end(); ++itChild)
	    {
	      int child_type = (*itChild)->getContentType();
#ifdef DEBUGLOAD
	      printf("   child_type = 0x%x\n", child_type);
#endif // DEBUGLOAD
	      switch(child_type)
		{
		case 0xa: // Event Number/Timestamp
		  {
		    const vector<uint64_t> *vec = (*itChild)->getVector<uint64_t>();
		    fEventNumber = (Int_t)((*vec)[0]);
		    fTimeStamp = (Int_t)((*vec)[1]);
		    break;
		  }
		
		case 0x5: // Event types
		  {
		    const vector<uint16_t> *vec = (*itChild)->getVector<uint16_t>();
		    fEventType = (*vec)[0];
		    break;
		  }
		
		case 0x1: // Extra TI data
		  break;

		default:
		  printf("%s: ERROR: Unknown data type %d for tag=0x%04x\n",
			 __func__, child_type, tag);
		}
	    }
#ifdef DEBUGLOAD
	  printf(" fEventNumber = 0x%x   fTimeStamp = 0x%x  fEventType = 0x%x\n",
		 fEventNumber, fTimeStamp, fEventType);
#endif // DEBUGLOAD

	  int iroc=0, iev=0;
	  aTrigger[iev].Clear();
	  // aTrigger[iev].fTID[iroc].tofb = iev;
	  // aTrigger[iev].fTID[iroc].eventHeader =
	  //   (unsigned int) tibank->data[index++];
	  aTrigger[iev].fTID[iroc].eventNumber = fEventNumber;
	  aTrigger[iev].fTID[iroc].timestamp = fTimeStamp;

	  aTrigger[iev].fTID[iroc].prev_eventNumber = prev_eventNumber[iroc];
	  aTrigger[iev].fTID[iroc].prev_timestamp = prev_timestamp[iroc];

	  if (aTrigger[iev].fTID[iroc].timestamp <
	      aTrigger[iev].fTID[iroc].prev_timestamp)
	    aTrigger[iev].fTID[iroc].rollover++;

	  UInt_t trigdiff;
	  
	  if(fTimeStamp < prev_timestamp[iroc])
	    trigdiff = ((1ULL)<<32) + prev_timestamp[iroc] - fTimeStamp;
	  else
	    trigdiff = fTimeStamp - prev_timestamp[iroc];
	
	  trigdiff /= (fEventNumber - prev_eventNumber[iroc]);

	  trigger_period->Fill(trigdiff);
	  
	  prev_eventNumber[iroc] = aTrigger[iev].fTID[iroc].eventNumber;
	  prev_timestamp[iroc] = aTrigger[iev].fTID[iroc].timestamp;

	}

      if(((tag == 500) || (tag == 0))
	 && (content_type == 0x10)) // ROC 0
	{
	  evioDOMNodeListP rocList = (*it)->getChildren();
	  for(std::list<evioDOMNodeP>::const_iterator itRoc = rocList->begin();
	      itRoc != rocList->end(); ++itRoc)
	    {
	      // int bank_type = (*itRoc)->getContentType();
	      fBlocklevel = (*itRoc)->num;
	      int bank_tag =  (*itRoc)->tag;
#ifdef DEBUGLOAD
	      printf("   bank_tag = 0x%x   type = %d   blocklevel = %d\n",
		     bank_tag, bank_type,
		     blocklevel);
#endif // DEBUGLOAD
	      switch(bank_tag)
		{
		case 0x1: // misc
		case 0x3: // fADC250
		case 0x5: // stats
		case 0x6: // CTP
		  {
		    bankmask |= (1<<bank_tag);
		    break;
		  }

		default:
		  printf("%s: ERROR: Unknown bank 0x%04x\n",
			 __func__, bank_tag);
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

      // FADC bank
      tn = evioDictEntry(3, fBlocklevel);
      if(bi.tagNumExists(tn))
	mBank[3] = (struct moduleBank *) (bi.getData < uint32_t > (tn, &len) -2);

      // CTP bank
      tn = evioDictEntry(6, fBlocklevel);
      if(bi.tagNumExists(tn))
	mBank[6] = (struct moduleBank *) (bi.getData < uint32_t > (tn, &len) -2);
      
      // Setup bank
      tn = evioDictEntry(5, 0);
      if(bi.tagNumExists(tn))
	mBank[5] = (struct moduleBank *) (bi.getData < uint32_t > (tn, &len) -2);

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
FCATAnalysis::End()
{
  // End of analysis.  
#ifdef DEBUG
  cout << "FCATAnalysis::End()" << endl;
#endif

  runstats->Write();
  evFile->Write();
  evFile->Close();

};

void
FCATAnalysis::Decode()
{
  UInt_t blocklevelTI = 0, blocklevelFADC, blocklevelCTP, stat;
  Bool_t foundData = kFALSE;
  
  
  aTrigger[0].Clear();

#ifdef OLDTIDECODE  
  if (mBank[4])	// TI Bank 
    {
      blocklevelTI = DecodeTI(0, 1, mBank[4], 1);
      if (blocklevelTI >= 1)
	foundData = kTRUE;
    }
#else
  blocklevelTI = fBlocklevel;
#endif // OLDTIDECODE  

  if (mBank[3])	// FADC Bank
    {
      blocklevelFADC = DecodeFADC(0, blocklevelTI, mBank[3], 1);
      foundData = kTRUE;
    }

  if (mBank[5])	// stats Bank
    {
      printf("%s plop\n", __func__);
      stat = DecodeSetup(mBank[5]);
      foundData = kTRUE;
    }

  if (mBank[6])	// CTP Bank
    {
      blocklevelCTP = DecodeCTP(0, 1, mBank[6], 1);
      foundData = kTRUE;
    }

  if (foundData)
    {
#ifdef DOTREE
      evTree->Fill();
      //       for(UInt_t iev=0; iev<blocklevelTI; iev++)
      for (UInt_t iev = 0; iev < 1; iev++)
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

int
FCATAnalysis::DecodeTI(int iroc, UInt_t blocklevel,
		       struct moduleBank *tibank, unsigned int rocnumber)
{
  unsigned int blocksize = 0;
  int index;
#ifdef OLDTIDECODE
#ifdef DEBUGTI
  printf("%s: will look for 0x%08x\n", __FUNCTION__,
	 ((1 << 28) | (21 << 16) | (rocnumber << 22)));
#endif

  // Search for the blockheader
  for (UInt_t iword = 0; iword < (tibank->length - 1); iword++)
    {
      //       if( (tibank->data[iword] & 0xFFFF0000) == ((1<<28) | (21<<16) | (rocnumber<<22)) )
      //       if((tibank->data[iword] & 0xFFC00000) == (UInt_t)((1<<31) | (20<<22)) )
      if (((*tibank->data)[iword] & 0xFFC00000) ==
	  (UInt_t) ((1 << 31) | (21 << 22)))
	{
	  blocksize = tibank->data[iword] * 0xFF;
	  index = iword;
	  break;
	}
    }
  if (blocksize == 0)
    {
      printf
	("%s: %6d: FAILED to find TI Block Header. data = 0x%08x\n",
	 __FUNCTION__, fEventNumber, tibank->data[1]);
      return -1;
    }

  TID[iroc].blockHeader1 = tibank->data[index++];	// 0x8540NNNN
  TID[iroc].blockHeader2 = tibank->data[index++];	// 0xFF11nnnn

  blocklevel = TID[iroc].blockHeader1 & 0xFF;

#ifdef DEBUGTI
  printf("%s: blocklevel = %d\n", __FUNCTION__, blocklevel);
#endif

  for (UInt_t iev = 0; iev < 1; iev++)
    {
      Long64_t tt1 = 0, tt2 = 0;
      aTrigger[iev].Clear();
      aTrigger[iev].fTID[iroc].tofb = iev;
      aTrigger[iev].fTID[iroc].eventHeader =
	(unsigned int) tibank->data[index++];
      aTrigger[iev].fTID[iroc].eventNumber =
	(unsigned int) tibank->data[index++];
      tt1 = (Long64_t) tibank->data[index++];
      tt2 = (Long64_t) tibank->data[index++] & 0xFFFF;
      aTrigger[iev].fTID[iroc].timestamp = (tt2 << 32) + tt1;
      aTrigger[iev].fTID[iroc].prev_eventNumber = prev_eventNumber[iroc];
      aTrigger[iev].fTID[iroc].prev_timestamp = prev_timestamp[iroc];
      if (aTrigger[iev].fTID[iroc].timestamp <
	  aTrigger[iev].fTID[iroc].prev_timestamp)
	aTrigger[iev].fTID[iroc].rollover++;
      prev_eventNumber[iroc] = aTrigger[iev].fTID[iroc].eventNumber;
      prev_timestamp[iroc] = aTrigger[iev].fTID[iroc].timestamp;
      // #define DEBUGTI
#ifdef DEBUGTI
      printf
	("debugti: ev = %d   tt1 = %llx  tt2 = %llx  total = %llx\n",
	 aTrigger[iev].fTID[iroc].eventNumber, tt1, tt2,
	 aTrigger[iev].fTID[iroc].timestamp);
#endif
    }
  TID[iroc].blockTrailer = (unsigned int) tibank->data[index++];


  return blocklevel;
#endif // OLDTIDECODE

}


int
FCATAnalysis::DecodeFADC(int iroc, UInt_t blocklevel,
			 struct moduleBank *fabank, unsigned int rocnumber)
{
  UInt_t iword = 0;
  UInt_t data = 0, data_type = 0;

  // Find the FADC header
  Bool_t fadcFound;
  Int_t slotid;
  Int_t fadcnum;

// #define DEBUGFADC
#ifdef DEBUGFADC
  printf("%s: Entering.. fabank->length = %d\n", __FUNCTION__, fabank->length);
#endif

  // Continue while the index word is less than the length of the bank
  while (iword < fabank->length)
    {
      data = 0;
      data_type = 0;

      fadcFound = kFALSE;
      slotid = 0;
      fadcnum = 0;

      // Locate the block header
      data = fabank->data[iword];
      data_type = (data & FA_DATA_TYPE_MASK) >> 27;
#ifdef DEBUGFADC
      printf("%4d: data = 0x%08x\n", iword, data);
#endif
      if ((data & FA_DATA_TYPE_DEFINE) &&
	  ((data & FA_DATA_TYPE_MASK) == FA_DATA_BLOCK_HEADER))
	{
	  slotid = (data & FA_DATA_SLOT_MASK) >> 22;
	  fadcnum = slotid;
	  FADC[fadcnum + iroc * 16].blockHeader = data;
	  FADC[fadcnum + iroc * 16].slotID = slotid;
	  fadcFound = kTRUE;
	  iword++;
	}

      if (fadcFound)
	{
#ifdef DEBUGFADC
	  printf("%4d: ROC %2d: FOUND FADC %d\n", iword, iroc, slotid);
#endif
	  UInt_t
	    fadc_blocklevel =
	    (FADC[fadcnum + iroc * 16].blockHeader & FA_DATA_BLKLVL_MASK);
	  if (blocklevel != fadc_blocklevel)
	    printf
	      ("ERROR: FADC Blocklevel != TID Blocklevel (%d != %d)\n",
	       fadc_blocklevel, blocklevel);

	  // Loop over buffer, filling as we find data
	  UInt_t ievent = 0;
	  Bool_t foundEvHeader = kFALSE;	// Found the 1st event header?
	  Bool_t allDone = kFALSE;
	  Int_t prev_data_type = 0;
	  while (iword < fabank->length)
	    {

	      data = fabank->data[iword];
	      data_type = (data & FA_DATA_TYPE_MASK) >> 27;
#ifdef DEBUGFADC
	      printf("%4d: data_type = %d\n", iword, data_type);
#endif
	      switch (data_type)
		{
		case 0:	// Continuation word... may happen if in raw data mode
		  switch (prev_data_type)
		    {
		    case 0:	// Expected...
		    case 4:	// Expected...
		      break;
		    default:	// Unexpected...
		      printf
			("ERROR: Unexpected continuation word.  Previous data type %d\n",
			 prev_data_type);
		    }
		case 1:	// Block Trailer... we're all done.
#ifdef DEBUGFADC
		  printf("%4d: Found Block Trailer\n", iword);
#endif
		  allDone = kTRUE;
		  break;
		case 2:	// Event Header
		  {
		    UInt_t
		      triggerNumber =
		      fabank->data[iword] & FA_DATA_TRIGNUM_MASK;
#ifdef DEBUGFADC
		    printf
		      ("%4d: Found Event Header  triggerNumber = %d\n",
		       iword, triggerNumber);
#endif
		    if (foundEvHeader)
		      {
			ievent++;	// If already found, incr event number
			allDone = kTRUE;	// just analyzing one in the block
			break;
		      }
		    foundEvHeader = kTRUE;

		    aTrigger[ievent].trignum[iroc][fadcnum] = triggerNumber;

		    aTrigger[ievent].blockevent = ievent;

		    break;
		  }
		case 3:	// Trigger Time
		  {
		    UInt_t
		      tt1 = (unsigned int)
		      (fabank->data[iword++] & FA_DATA_TRIGTIME_MASK);
		    UInt_t
		      tt2 = (unsigned int)
		      (fabank->data[iword] & FA_DATA_TRIGTIME_MASK);
		    aTrigger[ievent].trigtime[iroc][fadcnum] =
		      ((Long64_t) tt2) << 24;
		    aTrigger[ievent].trigtime[iroc][fadcnum] += (Long64_t) tt1;

		    FADC[fadcnum + iroc * 16].triggerTime =
		      aTrigger[ievent].trigtime[iroc][fadcnum];

		    Long64_t diff
		      = (aTrigger[ievent].fTID[iroc].timestamp
			 + aTrigger[ievent].fTID[iroc].rollover*((1ULL)<<32))
		      - aTrigger[ievent].trigtime[iroc][fadcnum];
		    timestamp_diff[fadcnum]->Fill(diff);
#ifdef DEBUGFADC
		    cout <<
		      Form
		      ("%2d  TriggerTime tt1 = %x  tt2 = %x trigtime = %llx",
		       fadcnum, tt1, tt2,
		       aTrigger[ievent].trigtime[iroc][fadcnum]) << endl;
#endif

		    break;
		  }
		case 4:	// Raw Window Data
		  {
		    UInt_t window_width = 0, data = 0;
		    UInt_t isample = 0;
		    UInt_t
		      chan = (fabank->data[iword] & FA_DATA_CHAN_MASK) >> 23;
		    window_width =
		      fabank->data[iword++] & FA_DATA_WINWIDTH_MASK;
#ifdef DEBUGFADC
		    cout << Form("%2d  chan = %d   window_width = %d",
				 fadcnum, chan, window_width) << endl;
#endif
		    if (chan > FADC_MAX_CHAN - 1)
		      {
			cerr << "chan " << chan <<
			  " greater than FADC_MAX_CHAN-1 " <<
			  FADC_MAX_CHAN - 1 << endl;
			break;
		      }
#ifdef OLDFIRMWARE

		    aTrigger[ievent].hitmod[iroc][fadcnum] = fadcnum;

		    aTrigger[ievent].hitchannel[iroc][fadcnum]
		      [aTrigger[ievent].fNhits[iroc][fadcnum]++] = chan + 1;
#endif
		    aTrigger[ievent].fmod[fadcnum].slot = fadcnum;
		    aTrigger[ievent].fmod[fadcnum].fchan[chan].chan = chan;

		    while ((fabank->data[iword] & FA_DATA_TYPE_DEFINE) == 0)
		      {
#ifdef DEBUGFADC
			cout <<
			  Form
			  ("%5d: ievent = %d  sample = %d  fadcnum = %d  0x%08x (data)",
			   iword, ievent, isample, fadcnum,
			   fabank->data[iword]) << endl;
#endif
			/* Two samples stored for each word */
			data = fabank->data[iword];

			aTrigger[ievent].fmod[fadcnum].
			  fchan[chan].raw_data[isample] =
			  ((data & 0X1FFF0000) >> 16);
			aTrigger[ievent].fmod[fadcnum].
			  fchan[chan].sample[isample] = isample;

			isample++;

			raw_data[fadcnum][chan]->Fill((data & 0X1FFF0000) >> 16);


			/* Second sample in the word */
			aTrigger[ievent].fmod[fadcnum].
			  fchan[chan].raw_data[isample] = (data & 0X1FFF);
			aTrigger[ievent].fmod[fadcnum].
			  fchan[chan].sample[isample] = isample;

			raw_data[fadcnum][chan]->Fill(data & 0X1FFF);

			if (isample > FADC_MAX_SAMPLES - 1)
			  {
			    cerr << "samples " << isample <<
			      " greater than FADC_MAX_SAMPLES-1 " <<
			      FADC_MAX_SAMPLES - 1 << endl;
			    break;
			  }

			isample++;

			iword++;
		      }

		    /* Store the total number of samples */
		    aTrigger[ievent].fmod[fadcnum].fchan[chan].nsamples =
		      isample;

		    /* Decrement, since the while loop incremented at the end */
		    iword--;

		    break;
		  }
		case 7:	// Pulse Integral
		  {
		    UInt_t
		      pulse_number = (data & FA_DATA_PULSE_NUMBER_MASK) >> 21;
		    UInt_t chan = (data & FA_DATA_CHAN_MASK) >> 23;
		    UInt_t integral = (data & FA_DATA_PULSE_INTEGRAL_MASK);

		    if (pulse_number > FADC_MAX_PULSES)
		      {
			printf("Too many pulses for eventnumber %d\n",
			       fEventNumber);
		      }
		    else
		      {
			aTrigger[ievent].fmod[fadcnum].
			  fchan[chan].pulse_integral[pulse_number] = integral;
			aTrigger[ievent].fmod[fadcnum].fchan[chan].nPulses =
			  pulse_number + 1;

			aTrigger[ievent].hitchannel[iroc][fadcnum]
			  [aTrigger[ievent].fNhits[iroc][fadcnum]++] = chan + 1;
			aTrigger[ievent].hitmod[iroc][fadcnum] = fadcnum;
		      }
		    break;
		  }
		case 8:	// Pulse Time Data
		  {
		    UInt_t
		      pulse_number = (data & FA_DATA_PULSE_NUMBER_MASK) >> 21;
		    UInt_t chan = (data & FA_DATA_CHAN_MASK) >> 23;
		    UInt_t time = (data & FA_DATA_PULSE_TIME_MASK);

		    if (pulse_number > FADC_MAX_PULSES)
		      {
			printf("Too many pulses for eventnumber %d\n",
			       fEventNumber);
		      }
		    else
		      {
			aTrigger[ievent].fmod[fadcnum].
			  fchan[chan].pulse_time[pulse_number] = time;
			aTrigger[ievent].fmod[fadcnum].fchan[chan].nPulses =
			  pulse_number + 1;
		      }
		    break;
		  }

		case 9:	/* PULSE PARAMETERS */
		  {
		    /* Channel ID and Pedestal Info */
		    Int_t pulse_number = -1;	/* Initialize */
		    UInt_t chan = (data & 0x00078000) >> 15;
		    UInt_t ped_sum = (data & 0x00003fff);
#ifdef DEBUGFADC
		    UInt_t evt_of_blk = (data & 0x07f80000) >> 19;
		    UInt_t quality = (data & (1 << 14)) >> 14;

		    printf
		      ("%8X - PULSEPARAM 1 - evt = %d   chan = %d   quality = %d   pedsum = %d\n",
		       data, evt_of_blk, chan, quality, ped_sum);
#endif
		    iword++;

		    aTrigger[ievent].fmod[fadcnum].fchan[chan].ped_sum =
		      ped_sum;

		    /* Loop over all pulses for this channel */
		    while ((fabank->data[iword] & FA_DATA_TYPE_DEFINE) == 0)
		      {
#ifdef DEBUGFADC
			cout <<
			  Form
			  ("%5d: ievent = %d  fadcnum = %d  0x%08x (data)",
			   iword, ievent, fadcnum, fabank->data[iword]) << endl;
#endif
			data = fabank->data[iword];

			if (data & (1 << 30))
			  {	/* Word 2: Integral of n-th pulse in window */
			    pulse_number++;
			    UInt_t adc_sum = (data & 0x3ffff000) >> 12;
#ifdef DEBUGFADC
			    UInt_t nsa_ext = (data & (1 << 11)) >> 11;
			    UInt_t over = (data & (1 << 10)) >> 10;
			    UInt_t under = (data & (1 << 9)) >> 9;
			    UInt_t samp_ov_thres = (data & 0x000001ff);

			    printf
			      ("%8X - PULSEPARAM 2 - P# = %d  Sum = %d  NSA+ = %d  Ov/Un = %d/%d  #OT = %d\n",
			       data, pulse_number, adc_sum, nsa_ext,
			       over, under, samp_ov_thres);
#endif
			    if (pulse_number > FADC_MAX_PULSES)
			      {
				printf
				  ("Too many pulses (%d) for eventnumber %d in channel %d\n",
				   pulse_number, fEventNumber, chan);
			      }
			    else
			      {
				aTrigger[ievent].fmod[fadcnum].
				  fchan[chan].pulse_integral[pulse_number] =
				  adc_sum;
				aTrigger[ievent].fmod[fadcnum].fchan[chan].
				  nPulses = pulse_number + 1;

				pulse_integral[fadcnum][chan]->Fill(adc_sum);


				aTrigger[ievent].hitchannel[iroc][fadcnum]
				  [aTrigger[ievent].fNhits[iroc][fadcnum]++] =
				  chan + 1;
				aTrigger[ievent].hitmod[iroc][fadcnum] =
				  fadcnum;

				mod_hit[fadcnum]->Fill(fadcnum);
				channel_hit[fadcnum][chan]->Fill(chan);
			      }
			  }
			else
			  {	/* Word 3: Time of n-th pulse in window */
			    UInt_t time_coarse = (data & 0x3fe00000) >> 21;
			    UInt_t time_fine = (data & 0x001f8000) >> 15;
#ifdef DEBUGFADC
			    UInt_t vpeak = (data & 0x00007ff8) >> 3;
			    UInt_t quality = (data & 0x2) >> 1;
			    UInt_t quality2 = (data & 0x1);

			    printf
			      ("%8X - PULSEPARAM 3 - CTime = %d  FTime = %d  Peak = %d  NoVp = %d  Q = %d\n",
			       data, time_coarse, time_fine, vpeak,
			       quality, quality2);
#endif
			    if (pulse_number > FADC_MAX_PULSES)
			      {
				printf
				  ("Too many pulses (%d) for eventnumber %d in channel %d\n",
				   pulse_number, fEventNumber, chan);
			      }
			    else
			      {
				aTrigger[ievent].fmod[fadcnum].
				  fchan[chan].pulse_time[pulse_number] =
				  ((time_coarse << 6) ) + time_fine;
				aTrigger[ievent].fmod[fadcnum].fchan[chan].
				  nPulses = pulse_number + 1;

				pulse_time[fadcnum][chan]->
				  Fill(((time_coarse << 6) ) + time_fine);
			      }
			  }
			iword++;
		      }
		    iword--;
		  }
		  break;

		case 15:	// Filler Word
		  break;

		default:
		  printf
		    ("ERROR: Undefined data type %d for event number %d\n",
		     data_type, fEventNumber);
		}
	      iword++;
	      prev_data_type = data_type;
	      if (allDone)
		break;
	    }
	}
      else
	iword++;


    }

  return blocklevel;
}

int
FCATAnalysis::DecodeF1TDC(int iroc,	// index of ROC in various arrays
			  UInt_t blocklevel, struct moduleBank *f1bank, unsigned int rocnumber	// rocnumber as reported by the TI
			  )
{
#ifdef SKIPF1
  UInt_t iword = 0;
  UInt_t data = 0, data_type = 0;

  // Find the F1TDC header
  Bool_t f1Found;
  Int_t slotid;
  Int_t f1num;
  Int_t rollover = runstats->f1Rollover;

  // #define DEBUGF1
#ifdef DEBUGF1
  printf("%s: Entering.. f1bank->length = %d\n", __FUNCTION__, f1bank->length);
#endif

  // Continue while the index word is less than the length of the bank
  while (iword < f1bank->length)
    {
      data = 0;
      data_type = 0;

      f1Found = kFALSE;
      slotid = 0;
      f1num = 0;

      // Locate the block header
      data = f1bank->data[iword];
      data_type = (data & FA_DATA_TYPE_MASK) >> 27;
#ifdef DEBUGF1
      printf("%4d: data = 0x%08x\n", iword, data);
#endif
      if ((data & FA_DATA_TYPE_DEFINE) &&
	  ((data & FA_DATA_TYPE_MASK) == FA_DATA_BLOCK_HEADER))
	{
	  slotid = (data & FA_DATA_SLOT_MASK) >> 22;
	  f1num = slotid;
	  F1TDC[f1num + iroc * 16].blockHeader = data;
	  F1TDC[f1num + iroc * 16].slotID = slotid;
	  f1Found = kTRUE;
	  iword++;
	}

      if (f1Found)
	{
#ifdef DEBUGF1
	  printf("%4d: ROC %2d: FOUND F1 %d\n", iword, iroc, slotid);
#endif
	  UInt_t
	    f1_blocklevel =
	    (F1TDC[f1num + iroc * 16].blockHeader & FA_DATA_BLKLVL_MASK) >> 11;
	  if (blocklevel != f1_blocklevel)
	    printf
	      ("ERROR: F1 Blocklevel != TID Blocklevel (%d != %d)\n",
	       f1_blocklevel, blocklevel);

	  // Loop over buffer, filling as we find data
	  UInt_t ievent = 0;
	  Bool_t foundEvHeader = kFALSE;	// Found the 1st event header?
	  Bool_t allDone = kFALSE;
	  Int_t prev_data_type = 0;

	  UInt_t chip_trigger_time = 0;

	  while (iword < f1bank->length)
	    {

	      data = f1bank->data[iword];
	      data_type = (data & FA_DATA_TYPE_MASK) >> 27;
#ifdef DEBUGF1
	      printf("%4d: data_type = %d\n", iword, data_type);
#endif
	      switch (data_type)
		{
		case 0:	// Continuation word... may happen if in raw data mode
		  switch (prev_data_type)
		    {
		    case 0:	// Expected...
		    case 4:	// Expected...
		      break;
		    default:	// Unexpected...
		      printf
			("ERROR: Unexpected continuation word.  Previous data type %d\n",
			 prev_data_type);
		    }
		case 1:	// Block Trailer... we're all done.
#ifdef DEBUGF1
		  printf("%4d: Found Block Trailer\n", iword);
#endif
		  allDone = kTRUE;
		  break;
		case 2:	// Event Header
		  {
		    UInt_t
		      triggerNumber =
		      f1bank->data[iword] & FA_DATA_TRIGNUM_MASK;
#ifdef DEBUGF1
		    printf
		      ("%4d: Found Event Header  triggerNumber = %d\n",
		       iword, triggerNumber);
#endif
		    if (foundEvHeader)
		      {
			ievent++;	// If already found, incr event number
			allDone = kTRUE;	// just analyzing one in the block
			break;
		      }
		    foundEvHeader = kTRUE;

		    aTrigger[ievent].trignum[iroc][f1num] = triggerNumber;

		    aTrigger[ievent].blockevent = ievent;

		    break;
		  }
		case 3:	// Trigger Time
		  {
		    UInt_t tt1 = (unsigned int) f1bank->data[iword++];
		    UInt_t tt2 = (unsigned int) f1bank->data[iword];
		    // FIXME: Fix these masks
		    aTrigger[ievent].trigtime[iroc][f1num] =
		      ((Long64_t) (tt1 & F1_DATA_TRIGTIME1_MASK));
		    aTrigger[ievent].trigtime[iroc][f1num] +=
		      (Long64_t) (tt2 & F1_DATA_TRIGTIME2_MASK) << 24;

		    F1TDC[f1num + iroc * 16].triggerTime =
		      aTrigger[ievent].trigtime[iroc][f1num];

		    break;
		  }

		case 7:	// F1 Event data
		  {
		    // FIXME: Fix the channel number decoding.
		    UInt_t time = (data & F1_DATA_HIT_TIME_MASK);
		    UInt_t chanaddr = (data & F1_DATA_HIT_CHAN_ADDR_MASK) >> 16;
		    UInt_t chipaddr = (data & F1_DATA_HIT_CHIP_ADDR_MASK) >> 19;

		    UInt_t type = runstats->f1Type;
		    UInt_t chan = DecodeF1Chan(type, chanaddr, chipaddr);
		    UInt_t
		      hitnum = aTrigger[ievent].f1mod[f1num].fchan[chan].nHits;
#ifdef DEBUGF1
		    printf
		      ("%s: chan = %d   f1num = %d     hitnum = %d\n",
		       __FUNCTION__, chan, f1num, hitnum);
#endif


		    if (hitnum + 1 > F1TDC_MAX_HITS)
		      {
			printf("Too many hits for eventnumber %d\n",
			       fEventNumber);
		      }
		    else
		      {
			aTrigger[ievent].f1mod[f1num].fchan[chan].time[hitnum] =
			  time;
			aTrigger[ievent].f1mod[f1num].
			  fchan[chan].trigtime[hitnum] = chip_trigger_time;
			aTrigger[ievent].f1mod[f1num].
			  fchan[chan].ctime[hitnum] =
			  time - chip_trigger_time +
			  ((time < chip_trigger_time) ? rollover : 0);

			aTrigger[ievent].f1mod[f1num].fchan[chan].nHits =
			  hitnum + 1;
			runstats->f1ChannelHits[0][f1num][chan]++;

			aTrigger[ievent].f1hitchannel[iroc][f1num]
			  [aTrigger[ievent].fNhits[iroc][f1num]++] = chan + 1;
			aTrigger[ievent].f1hitmod[iroc][f1num] = f1num;
		      }
		    break;
		  }

		case 8:	// F1 Chip Header
		  {
		    chip_trigger_time = (data & F1_DATA_CHIP_TRIGGER_TIME_MASK);
		    UInt_t chip = (data & F1_DATA_CHIP_ADDR_MASK) >> 3;
		    UInt_t errormask = (data & F1_DATA_CHIP_ERROR_MASK) >> 24;
		    aTrigger[ievent].f1ErrCode[0][f1num][chip] = errormask;
		    if (errormask != F1_DATA_NO_ERROR)
		      runstats->f1Errors[0][f1num][chip]++;

		    break;
		  }
		case 15:	// Filler Word
		  break;

		case 4:
		default:
		  printf
		    ("ERROR: Undefined data type %d for event number %d\n",
		     data_type, fEventNumber);
		}
	      iword++;
	      prev_data_type = data_type;
	      if (allDone)
		break;
	    }
	}
      else
	iword++;


    }

#endif // SKIPF1
  return blocklevel;
}

int
FCATAnalysis::DecodeCTP
(int iroc,
 UInt_t blocklevel, struct moduleBank *ctpbank, unsigned int rocnumber)
{
#ifdef SKIPCTP
  UInt_t
    iword = 0;
  UInt_t
    data = 0;

#ifdef DEBUGCTP
  printf("%s: Entering.. ctpbank->length = %d\n",
	 __FUNCTION__, ctpbank->length);
#endif

  while (iword < (ctpbank->length - 2))
    {
      if (iword > CTP_MAX_SAMPLES)
	{
	  printf("%s: ERROR: Too many CTP samples to process!\n", __FUNCTION__);
	  break;
	}

      data = ctpbank->data[iword];
      aTrigger[0].fCTP.data[iword] = data & 0xFFFFF;
      aTrigger[0].fCTP.sample[iword] = iword;

      iword++;
    }
  aTrigger[0].fCTP.nsamples = iword;
#endif // SKIPCTP
  return blocklevel;
}

void
FCATAnalysis::PrintBuffer(const uint32_t * buffer)
{
  // Print the event buffer for diagnostics.
  int len = buffer[0] + 1, idata;;
  int evtype = buffer[1] >> 16;
  int evnum = buffer[4];
  cout << "\n\n Event number " << dec << evnum;
  cout << " length " << len << " type " << evtype << endl;
  int ipt = 0;
  for (int j = 0; j < (len / 5); j++)
    {
      cout << dec << "\n evbuffer[" << ipt <<
	"] = ";
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
FCATAnalysis::PrintData()
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

Bool_t FCATAnalysis::IsPrestartEvent() const
{
  Bool_t
    rval = kFALSE;

  if (fCodaEvTag == 0xFFD1)
    {
      rval = kTRUE;
    }

  return rval;
}

Bool_t FCATAnalysis::IsPhysicsEvent() const 
{
  Bool_t
    rval = kFALSE;

  if ((fCodaEvTag >= 0xFF50) && (fCodaEvTag <= 0xFF8F))
    {
      rval = kTRUE;
    }

  return rval;
}

Bool_t FCATAnalysis::IsSetupEvent() const
{
  // Leave this in here for now... but it's always false.
  return kFALSE;

  return (fEvType == 13);
}



Int_t FCATAnalysis::rocIndexFromNumber(int rocNumber)
{
  static int
    initd = 0;
  static int
    rocIndex[NROCS];

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

UInt_t FCATAnalysis::DecodeSetup(struct moduleBank *setupbank)
{
  UInt_t ibuf;
// #define DEBUGSETUP
#ifdef DEBUGSETUP
  printf("%s: Entering.. setupbank->length = %d\n",
	 __FUNCTION__, setupbank->length);
  for (int i = 0; i < setupbank->length; i++)
    printf("%s: data[%2d] = 0x%08x\n",
	   __FUNCTION__, i, setupbank->data[i]);
#endif

  ibuf = 0;
  
  runstats->PayloadType        = setupbank->data[ibuf++];
  runstats->PayloadIntegral    = setupbank->data[ibuf++];

  runstats->Latency   = setupbank->data[ibuf++];
  runstats->Window    = setupbank->data[ibuf++];


  runstats->ScanMask  = setupbank->data[ibuf++];
  runstats->nMod      = setupbank->data[ibuf++];
  
#ifdef DEBUGSETUP
  printf(" PayloadType = 0x%x  PayloadIntegral = %d\n",
	 runstats->PayloadType,
	 runstats->PayloadIntegral);
  printf(" Latency = %d   Window = %d\n",
	 runstats->Latency, runstats->Window);
  printf(" ScanMask = 0x%08x   nMod = %d\n",
	 runstats->ScanMask, runstats->nMod);
#endif
	     while (ibuf < (setupbank->length - 1))
    {
      UInt_t slot = setupbank->data[ibuf] >> 19;
      UInt_t number = setupbank->data[ibuf++] & 0xFFF;

      runstats->SerialNumber[slot]       = Form("ACDI-%03d", number);
      runstats->FirmwareVersion[slot]    = setupbank->data[ibuf++];
      runstats->PayloadChannelMask[slot] = setupbank->data[ibuf++];

#ifdef DEBUGSETUP
      printf("%2d: %2d: %s   0x%08x  0x%04x\n",
	     ibuf, slot,
	     runstats->SerialNumber[slot].Data(),
	     runstats->FirmwareVersion[slot],
	     runstats->PayloadChannelMask[slot]);
#endif
}

  return 0;
}

UInt_t FCATAnalysis::DecodeF1Chan(UInt_t type, UInt_t chanaddr, UInt_t chipaddr)
{
  UInt_t
    rchan = 0;
  UInt_t
    v2_chanmap[8] = { 0, 0, 1, 1, 2, 2, 3, 3 };

  switch (type)
    {
    case 2:
      rchan = (4 * chipaddr) + v2_chanmap[chanaddr];
      break;

    case 3:
      rchan = (chipaddr << 3) | chanaddr;
      break;

    default:
      printf("%s: Invalid F1 Module type %d\n", __FUNCTION__, type);
    }

  return rchan;
}


void
FCATAnalysis::faDataDecode(unsigned int data)
{
  int i_print = 1;
  static unsigned int type_last = 15;	/* initialize to type FILLER WORD */
  static unsigned int time_last = 0;
  static unsigned int iword = 0;
  fadc_data_struct fadc_data;

  printf("%3d: ", iword++);

  if (data & 0x80000000)	/* data type defining word */
    {
      fadc_data.new_type = 1;
      fadc_data.type = (data & 0x78000000) >> 27;
    }
  else
    {
      fadc_data.new_type = 0;
      fadc_data.type = type_last;
    }

  switch (fadc_data.type)
    {
    case 0:			/* BLOCK HEADER */
      fadc_data.slot_id_hd = ((data) & 0x7C00000) >> 22;
      fadc_data.n_evts = (data & 0x3FF800) >> 11;
      fadc_data.blk_num = (data & 0x7FF);
      if (i_print)
	printf("%8X - BLOCK HEADER - slot = %d   n_evts = %d   n_blk = %d\n",
	       data, fadc_data.slot_id_hd, fadc_data.n_evts, fadc_data.blk_num);
      break;
    case 1:			/* BLOCK TRAILER */
      fadc_data.slot_id_tr = (data & 0x7C00000) >> 22;
      fadc_data.n_words = (data & 0x3FFFFF);
      if (i_print)
	printf("%8X - BLOCK TRAILER - slot = %d   n_words = %d\n",
	       data, fadc_data.slot_id_tr, fadc_data.n_words);
      break;
    case 2:			/* EVENT HEADER */
      if (fadc_data.new_type)
	{
	  fadc_data.evt_num_1 = (data & 0x7FFFFFF);
	  if (i_print)
	    printf("%8X - EVENT HEADER 1 - evt_num = %d\n", data,
		   fadc_data.evt_num_1);
	}
      else
	{
	  fadc_data.evt_num_2 = (data & 0x7FFFFFF);
	  if (i_print)
	    printf("%8X - EVENT HEADER 2 - evt_num = %d\n", data,
		   fadc_data.evt_num_2);
	}
      break;
    case 3:			/* TRIGGER TIME */
      if (fadc_data.new_type)
	{
	  fadc_data.time_1 = (data & 0xFFFFFF);
	  if (i_print)
	    printf("%8X - TRIGGER TIME 1 - time = %08x\n", data,
		   fadc_data.time_1);
	  fadc_data.time_now = 1;
	  time_last = 1;
	}
      else
	{
	  if (time_last == 1)
	    {
	      fadc_data.time_2 = (data & 0xFFFFFF);
	      if (i_print)
		printf("%8X - TRIGGER TIME 2 - time = %08x\n", data,
		       fadc_data.time_2);
	      fadc_data.time_now = 2;
	    }
	  else if (time_last == 2)
	    {
	      fadc_data.time_3 = (data & 0xFFFFFF);
	      if (i_print)
		printf("%8X - TRIGGER TIME 3 - time = %08x\n", data,
		       fadc_data.time_3);
	      fadc_data.time_now = 3;
	    }
	  else if (time_last == 3)
	    {
	      fadc_data.time_4 = (data & 0xFFFFFF);
	      if (i_print)
		printf("%8X - TRIGGER TIME 4 - time = %08x\n", data,
		       fadc_data.time_4);
	      fadc_data.time_now = 4;
	    }
	  else if (i_print)
	    printf("%8X - TRIGGER TIME - (ERROR)\n", data);

	  time_last = fadc_data.time_now;
	}
      break;
    case 4:			/* WINDOW RAW DATA */
      if (fadc_data.new_type)
	{
	  fadc_data.chan = (data & 0x7800000) >> 23;
	  fadc_data.width = (data & 0xFFF);
	  if (i_print)
	    printf("%8X - WINDOW RAW DATA - chan = %d   nsamples = %d\n",
		   data, fadc_data.chan, fadc_data.width);
	}
      else
	{
	  fadc_data.valid_1 = 1;
	  fadc_data.valid_2 = 1;
	  fadc_data.adc_1 = (data & 0x1FFF0000) >> 16;
	  if (data & 0x20000000)
	    fadc_data.valid_1 = 0;
	  fadc_data.adc_2 = (data & 0x1FFF);
	  if (data & 0x2000)
	    fadc_data.valid_2 = 0;
	  if (i_print)
	    printf
	      ("%8X - RAW SAMPLES - valid = %d  adc = %4d   valid = %d  adc = %4d\n",
	       data, fadc_data.valid_1, fadc_data.adc_1, fadc_data.valid_2,
	       fadc_data.adc_2);
	}
      break;
    case 5:			/* WINDOW SUM */
      fadc_data.over = 0;
      fadc_data.chan = (data & 0x7800000) >> 23;
      fadc_data.adc_sum = (data & 0x3FFFFF);
      if (data & 0x400000)
	fadc_data.over = 1;
      if (i_print)
	printf("%8X - WINDOW SUM - chan = %d   over = %d   adc_sum = %08x\n",
	       data, fadc_data.chan, fadc_data.over, fadc_data.adc_sum);
      break;
    case 6:			/* PULSE RAW DATA */
      if (fadc_data.new_type)
	{
	  fadc_data.chan = (data & 0x7800000) >> 23;
	  fadc_data.pulse_num = (data & 0x600000) >> 21;
	  fadc_data.thres_bin = (data & 0x3FF);
	  if (i_print)
	    printf
	      ("%8X - PULSE RAW DATA - chan = %d   pulse # = %d   threshold bin = %d\n",
	       data, fadc_data.chan, fadc_data.pulse_num, fadc_data.thres_bin);
	}
      else
	{
	  fadc_data.valid_1 = 1;
	  fadc_data.valid_2 = 1;
	  fadc_data.adc_1 = (data & 0x1FFF0000) >> 16;
	  if (data & 0x20000000)
	    fadc_data.valid_1 = 0;
	  fadc_data.adc_2 = (data & 0x1FFF);
	  if (data & 0x2000)
	    fadc_data.valid_2 = 0;
	  if (i_print)
	    printf
	      ("%8X - PULSE RAW SAMPLES - valid = %d  adc = %d   valid = %d  adc = %d\n",
	       data, fadc_data.valid_1, fadc_data.adc_1, fadc_data.valid_2,
	       fadc_data.adc_2);
	}
      break;
    case 7:			/* PULSE INTEGRAL */
      fadc_data.chan = (data & 0x7800000) >> 23;
      fadc_data.pulse_num = (data & 0x600000) >> 21;
      fadc_data.quality = (data & 0x180000) >> 19;
      fadc_data.integral = (data & 0x7FFFF);
      if (i_print)
	printf
	  ("%8X - PULSE INTEGRAL - chan = %d   pulse # = %d   quality = %d   integral = %d\n",
	   data, fadc_data.chan, fadc_data.pulse_num, fadc_data.quality,
	   fadc_data.integral);
      break;
    case 8:			/* PULSE TIME */
      fadc_data.chan = (data & 0x7800000) >> 23;
      fadc_data.pulse_num = (data & 0x600000) >> 21;
      fadc_data.quality = (data & 0x180000) >> 19;
      fadc_data.time = (data & 0xFFFF);
      if (i_print)
	printf
	  ("%8X - PULSE TIME - chan = %d   pulse # = %d   quality = %d   time = %d\n",
	   data, fadc_data.chan, fadc_data.pulse_num, fadc_data.quality,
	   fadc_data.time);
      break;
    case 9:			/* STREAMING RAW DATA */
      if (fadc_data.new_type)
	{
	  fadc_data.chan_a = (data & 0x3C00000) >> 22;
	  fadc_data.source_a = (data & 0x4000000) >> 26;
	  fadc_data.chan_b = (data & 0x1E0000) >> 17;
	  fadc_data.source_b = (data & 0x200000) >> 21;
	  if (i_print)
	    printf
	      ("%8X - STREAMING RAW DATA - ena A = %d  chan A = %d   ena B = %d  chan B = %d\n",
	       data, fadc_data.source_a, fadc_data.chan_a, fadc_data.source_b,
	       fadc_data.chan_b);
	}
      else
	{
	  fadc_data.valid_1 = 1;
	  fadc_data.valid_2 = 1;
	  fadc_data.adc_1 = (data & 0x1FFF0000) >> 16;
	  if (data & 0x20000000)
	    fadc_data.valid_1 = 0;
	  fadc_data.adc_2 = (data & 0x1FFF);
	  if (data & 0x2000)
	    fadc_data.valid_2 = 0;
	  fadc_data.group = (data & 0x40000000) >> 30;
	  if (fadc_data.group)
	    {
	      if (i_print)
		printf
		  ("%8X - RAW SAMPLES B - valid = %d  adc = %d   valid = %d  adc = %d\n",
		   data, fadc_data.valid_1, fadc_data.adc_1,
		   fadc_data.valid_2, fadc_data.adc_2);
	    }
	  else if (i_print)
	    printf
	      ("%8X - RAW SAMPLES A - valid = %d  adc = %d   valid = %d  adc = %d\n",
	       data, fadc_data.valid_1, fadc_data.adc_1, fadc_data.valid_2,
	       fadc_data.adc_2);
	}
      break;
    case 10:			/* PULSE AMPLITUDE DATA */
      fadc_data.chan = (data & 0x7800000) >> 23;
      fadc_data.pulse_num = (data & 0x600000) >> 21;
      fadc_data.vmin = (data & 0x1FF000) >> 12;
      fadc_data.vpeak = (data & 0xFFF);
      if (i_print)
	printf
	  ("%8X - PULSE V - chan = %d   pulse # = %d   vmin = %d   vpeak = %d\n",
	   data, fadc_data.chan, fadc_data.pulse_num, fadc_data.vmin,
	   fadc_data.vpeak);
      break;

    case 11:			/* INTERNAL TRIGGER WORD */
      fadc_data.trig_type_int = data & 0x7;
      fadc_data.trig_state_int = (data & 0x8) >> 3;
      fadc_data.evt_num_int = (data & 0xFFF0) >> 4;
      fadc_data.err_status_int = (data & 0x10000) >> 16;
      if (i_print)
	printf
	  ("%8X - INTERNAL TRIGGER - type = %d   state = %d   num = %d   error = %d\n",
	   data, fadc_data.trig_type_int, fadc_data.trig_state_int,
	   fadc_data.evt_num_int, fadc_data.err_status_int);
    case 12:			/* UNDEFINED TYPE */
      if (i_print)
	printf("%8X - UNDEFINED TYPE = %d\n", data, fadc_data.type);
      break;
    case 13:			/* END OF EVENT */
      if (i_print)
	printf("%8X - END OF EVENT = %d\n", data, fadc_data.type);
      break;
    case 14:			/* DATA NOT VALID (no data available) */
      if (i_print)
	printf("%8X - DATA NOT VALID = %d\n", data, fadc_data.type);
      break;
    case 15:			/* FILLER WORD */
      if (i_print)
	printf("%8X - FILLER WORD = %d\n", data, fadc_data.type);
      break;
    }

  type_last = fadc_data.type;	/* save type of current data word */

}

void
FCATAnalysis::f1DataDecode(unsigned int data)
{
  static unsigned int type_last = 15;	/* initialize to type FILLER WORD */
  // static unsigned int time_last = 0;

  unsigned int data_type, slot_id_hd, slot_id_tr, blk_evts, blk_num, blk_words;
  unsigned int new_type, evt_num, time_1, time_2;
  // int rev=0;
  int mode = 0, factor = 0;

  //   rev = (f1Rev[id] & F1_VERSION_BOARDREV_MASK)>>16;
  //   if(rev==2)
  //     mode = 1;

  factor = 2 - mode;

  if (data & 0x80000000)	/* data type defining word */
    {
      new_type = 1;
      data_type = (data & 0x78000000) >> 27;
    }
  else
    {
      new_type = 0;
      data_type = type_last;
    }

  switch (data_type)
    {
    case 0:			/* BLOCK HEADER */
      slot_id_hd = (data & 0x7C00000) >> 22;
      blk_evts = (data & 0x3FF800) >> 11;
      blk_num = (data & 0x7FF);
      printf
	("      %08X - BLOCK HEADER  - slot = %u   blk_evts = %u   n_blk = %u\n",
	 data, slot_id_hd, blk_evts, blk_num);
      break;

    case 1:			/* BLOCK TRAILER */
      slot_id_tr = (data & 0x7C00000) >> 22;
      blk_words = (data & 0x3FFFFF);
      printf("      %08X - BLOCK TRAILER - slot = %u   n_words = %u\n",
	     data, slot_id_tr, blk_words);
      break;

    case 2:			/* EVENT HEADER */
      evt_num = (data & 0x7FFFFFF);
      printf("      %08X - EVENT HEADER - evt_num = %u\n", data, evt_num);
      break;

    case 3:			/* TRIGGER TIME */
      if (new_type)
	{
	  time_1 = (data & 0xFFFFFF);
	  printf("      %08X - TRIGGER TIME 1 - time = %u\n", data, time_1);
	  type_last = 3;
	}
      else
	{
	  if (type_last == 3)
	    {
	      time_2 = (data & 0xFFFF);
	      printf("      %08X - TRIGGER TIME 2 - time = %u\n", data, time_2);
	    }
	  else
	    printf("      %08X - TRIGGER TIME - (ERROR)\n", data);
	}
      break;

    case 7:			/* EVENT DATA */
      printf("TDC = %08X   ED   ERR=%X  chip=%u  chan=%u  t = %u (%u ps)\n", data, ((data >> 24) & 0x7),	// ERR
	     ((data >> 19) & 0x7),	// chip
	     ((data >> 16) & 0x7),	// chan
	     (data & 0xFFFF),	// t
	     ((data & 0xFFFF) * 56 * factor));
      break;

    case 8:			/* CHIP HEADER */
      /* need 2 prints - maximum # of variables is 7 in VxWorks printf (?) */
      printf("TDC = %08X --CH-- (%X,%u)  chip=%u  chan=%u  trig = %u  t = %3u ", data, ((data >> 24) & 0x7), ((data >> 6) & 0x1), ((data >> 3) & 0x7),	// chip
	     (data & 0x7),	//chan
	     ((data >> 16) & 0x3F),	// trig
	     ((data >> 7) & 0x1FF));	// t
      printf("(%u ps) (P=%u)\n",
	     (((data >> 7) & 0x1FF) * 56 * factor * 128), ((data >> 6) & 0x1));
      break;

    case 13:			/* EVENT TRAILER */
      /* need 2 prints - maximum # of variables is 7 in VxWorks printf (?) */
      printf
	("TDC = %08X --ET-- (%08X,%u)  chip=%u  chan=%u  trig = %u  t = %3u ",
	 data, ((data >> 24) & 0x7), ((data >> 6) & 0x1), ((data >> 3) & 0x7),
	 (data & 0x7), ((data >> 16) & 0x3F), ((data >> 7) & 0x1FF));
      printf("(%u ps) (P=%u)\n", (((data >> 7) & 0x1FF) * 56 * factor * 128),
	     ((data >> 6) & 0x1));
      break;

    case 14:			/* DATA NOT VALID (no data available) */
      printf("      %08X - DATA NOT VALID = %u\n", data, data_type);
      break;
    case 15:			/* FILLER WORD */
      printf("      %08X - FILLER WORD = %u\n", data, data_type);
      break;

    case 4:			/* UNDEFINED TYPE */
    case 5:			/* UNDEFINED TYPE */
    case 6:			/* UNDEFINED TYPE */
    case 9:			/* UNDEFINED TYPE */
    case 10:			/* UNDEFINED TYPE */
    case 11:			/* UNDEFINED TYPE */
    case 12:			/* UNDEFINED TYPE */
    default:
      printf("      %08X - UNDEFINED TYPE = %u\n", data, data_type);
      break;
    }

}
