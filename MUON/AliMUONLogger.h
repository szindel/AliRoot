#ifndef ALIMUONLOGGER_H
#define ALIMUONLOGGER_H

/* Copyright(c) 1998-1999, ALICE Experiment at CERN, All rights reserved. *
* See cxx source for full Copyright notice                               */

// $Id$

/// \ingroup base
/// \class AliMUONLogger
/// \brief A logger that keeps track of the number of times a message appeared
/// 
//  Author Laurent Aphecetche

#ifndef ROOT_TObject
#  include "TObject.h"
#endif

class AliMUONStringIntMap;

class AliMUONLogger : public TObject
{
public:
  AliMUONLogger(Int_t maxNumberOfEntries=-1);
  virtual ~AliMUONLogger();
  
  Int_t Log(const char* message);
  
  void Print(Option_t* opt="") const;
  
private:
  /// Not implemented
  AliMUONLogger(const AliMUONLogger& rhs); // not implemented
  /// Not implemented
  AliMUONLogger& operator=(const AliMUONLogger& rhs); // not implemented
  
private:
  
  Int_t fMaxNumberOfEntries; //!< after this number, print and reset
  AliMUONStringIntMap* fLog; //!< map from message to number of times the message was issued
  
  ClassDef(AliMUONLogger,1) // A logger that keeps track of the number of times a message appeared
};

#endif
