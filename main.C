// main.C
// Bryan Moffit, April, 2017
// Very simple analysis of TD data.

#include <iostream>
#include <stdio.h>
#include "evioUtil.hxx"
#include "evioFileChannel.hxx"
#include "evioBankIndex.hxx"
#include "TROOT.h"
#include "TRint.h"
#include "TString.h"
#include "tdAnalysis.h"

using namespace evio;
using namespace std;

void usage();			// print the usage instructions
TString getFileName(int);	// Get the filename from environment var.

int
main(int argc, char *argv[])
{

  evioFileChannel *chan;
  TROOT myana("fcat", "td Analysis");
  TString filename;
  int runnum = 0;
  char *cfilename;
  Int_t i = 1;
  int numevents = 0;
  int debugchoice = 0;
  int skipchoice = 0;
  uint32_t *buffer, blen;
  int choice_error = 0;

  cout << endl;
  cout << "TD Analysis - Bryan Moffit" << endl;

  if (argc < 3 || argc > 6)
    {
      cerr << "\nmain Error: Wrong number of arguments" << endl;
      usage();
      choice_error = 1;
      goto SKIPARGS;

    }

  while (i < argc)
    {
      if (strcasecmp(argv[i], "-r") == 0)
	{
	  if (i < argc - 1)
	    {
	      runnum = atoi(argv[++i]);
	      filename = getFileName(runnum);
	    }
	  else
	    {
	      choice_error = 1;
	      goto SKIPARGS;
	    }
	}
      else if (strcasecmp(argv[i], "-f") == 0)
	{
	  if (i < argc - 1)
	    {
	      cfilename = new char[strlen(argv[++i]) + 1];
	      strcpy(cfilename, argv[i]);
	      filename = cfilename;
	    }
	  else
	    {
	      choice_error = 1;
	    }
	}
      else if (strcasecmp(argv[i], "-n") == 0)
	{
	  if (i < argc - 1)
	    {
	      numevents = atoi(argv[++i]);
	    }
	  else
	    {
	      choice_error = 1;
	    }
	}
      else if (strcasecmp(argv[i], "-d") == 0)
	{
	  debugchoice = 1;
	  cout << "DEBUG option specified" << endl;
	}
      else if (strcasecmp(argv[i], "-s") == 0)
	{
	  skipchoice = 1;
	  cout << "SKIP option specified" << endl;
	}
      ++i;
    }

SKIPARGS:

  if (choice_error == 1)
    {
      cout << "Improper Usage" << endl;
      usage();
      return -1;
    }


  tdAnalysis *analysis = new tdAnalysis(debugchoice);	// Analysis

  cout << "CODA Data Filename: " << filename << endl;

  try
  {
    chan = new evioFileChannel(filename.Data(), "r");
  }
  catch(evioException e)
  {
    cerr << e.toString() << endl;
    cout << "ERROR: Cannot open CODA file.  Exiting." << filename << endl;
    return -1;
  }

  try
  {
    chan->open();
  }
  catch(evioException e)
  {
    cerr << e.toString() << endl;
    cout << "ERROR: Cannot open CODA file.  Exiting." << filename << endl;
    return -1;
  }

  // Send first event, hopefully prestart event, to analysis.Init()
  try
  {
    chan->readAlloc((uint32_t **) & buffer, &blen);
  }
  catch(evioException e)
  {
    cerr << e.toString() << endl;
    cout << "ERROR with realAlloc()" << endl;
    return -1;
  }

  analysis->Init((int *) buffer, skipchoice);
  free(buffer);

  // Loop over events
  int evnum = 0;
  bool status;

  if (numevents == 0)
    {
      // Loop until the end of the run
      while (chan->readAlloc((uint32_t **) & buffer, &blen))
	{
	  analysis->Process((int *) buffer);
	  if (((evnum + 1) % 1000) == 0)
	    {
	      cout << "Analyzed " << evnum + 1 << " events" << endl;
	    }
	  evnum++;
	}
    }
  else
    {
      // Loop until specified events is obtained
      for (int iev = 0; iev < (numevents + 2); iev++)
	{
	  status = chan->readAlloc((uint32_t **) & buffer, &blen);
	  if (!status)
	    {
	      cout << "Total Events Analyzed = " << dec << evnum - 2 << endl;
	      delete buffer;
	      chan->close();
	      analysis->End();
	      return 0;

	    }
	  else
	    {
	      analysis->Process((int *) buffer);
	      if (((iev + 1) % 1000) == 0)
		{
		  cout << "Analyzed " << iev + 1 << " events" << endl;
		}

	      evnum++;
	    }
	}
    }
  delete buffer;

  cout << "Total Events Analyzed = " << dec << evnum - 2 << endl;
  // Cleanup at the end
  chan->close();
  analysis->End();
  return 0;
};


void
usage()
{
  cout << "\nUsage: tan [data source specifier] [-n]" << endl;
  cout << "  where data source specifier is " << endl;
  cout << "   -r runnumber" << endl;
  cout << "   -f filepath" << endl;
  cout << "  Other Options:" << endl;
  cout << "   -n maxevents" << endl;
  cout << "      if 0 or unspecified, will analyze entire run" << endl;
  cout << "   -d" << endl;
  cout << "      for debugging purposes" << endl;
  cout << endl;
};

TString
getFileName(int num)
{
  TString fname;
  char *dataPath = getenv("CODA_DATA");
  if (dataPath == 0)
    {
      fname = "/home/data";
      cout << "Environment Variable CODA_DATA not defined.  Using default."
	<< endl;
    }
  else
    {
      fname = dataPath;
    }

  fname += Form("/test_4rocs_2dc_%d.evt.0", num);

  return fname;

  // Hack to make the warning: unused variable from evioUtils.hxx go away.
  cout << DataTypeNames[0] << endl;
}
