/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkDebugLeaks.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkDebugLeaks.h"

#include "vtkCriticalSection.h"
#include "vtkObjectFactory.h"
#include "vtkWindows.h"

#include <string>
#include <unordered_map>

static const char *vtkDebugLeaksIgnoreClasses[] = {
  nullptr
};

//----------------------------------------------------------------------------
// return 1 if the class should be ignored
static int vtkDebugLeaksIgnoreClassesCheck(const char* s)
{
  int i = 0;
  while(vtkDebugLeaksIgnoreClasses[i])
  {
    if(strcmp(s, vtkDebugLeaksIgnoreClasses[i]) == 0)
    {
      return 1;
    }
    i++;
  }
  return 0;
}

vtkStandardNewMacro(vtkDebugLeaks);

//----------------------------------------------------------------------------
class vtkDebugLeaksHashTable
{
public:
  vtkDebugLeaksHashTable() {}
  ~vtkDebugLeaksHashTable() {}
  void IncrementCount(const char *name);
  vtkTypeBool DecrementCount(const char* name);
  void PrintTable(std::string &os);
  bool IsEmpty();

private:
  std::unordered_map<const char *, unsigned int> CountMap;
};

//----------------------------------------------------------------------------
void vtkDebugLeaksHashTable::IncrementCount(const char * key)
{
  this->CountMap[key]++;
}

//----------------------------------------------------------------------------
bool vtkDebugLeaksHashTable::IsEmpty()
{
  return this->CountMap.empty();
}

//----------------------------------------------------------------------------
vtkTypeBool vtkDebugLeaksHashTable::DecrementCount(const char *key)
{
  if (this->CountMap.count(key) > 0)
  {
    this->CountMap[key]--;
    if (this->CountMap[key] == 0)
    {
      this->CountMap.erase(key);
    }
    return 1;
  }
  else
  {
    return 0;
  }
}

//----------------------------------------------------------------------------
void vtkDebugLeaksHashTable::PrintTable(std::string &os)
{
  auto iter = this->CountMap.begin();
  while (iter != this->CountMap.end())
  {
    if (iter->second > 0 && !vtkDebugLeaksIgnoreClassesCheck(iter->first))
    {
      char tmp[256];
      snprintf(tmp, 256, "\" has %i %s still around.\n", iter->second,
               (iter->second == 1) ? "instance" : "instances");
      os += "Class \"";
      os += iter->first;
      os += tmp;
    }
    ++iter;
  }
}

//----------------------------------------------------------------------------
#ifdef VTK_DEBUG_LEAKS
void vtkDebugLeaks::ConstructClass(const char* name)
{
  vtkDebugLeaks::CriticalSection->Lock();
  vtkDebugLeaks::MemoryTable->IncrementCount(name);
  vtkDebugLeaks::CriticalSection->Unlock();
}
#else
void vtkDebugLeaks::ConstructClass(const char*)
{
}
#endif

//----------------------------------------------------------------------------
#ifdef VTK_DEBUG_LEAKS
void vtkDebugLeaks::DestructClass(const char* p)
{
  vtkDebugLeaks::CriticalSection->Lock();
  // Due to globals being deleted, this table may already have
  // been deleted.
  if(vtkDebugLeaks::MemoryTable &&
     !vtkDebugLeaks::MemoryTable->DecrementCount(p))
  {
    vtkDebugLeaks::CriticalSection->Unlock();
    vtkGenericWarningMacro("Deleting unknown object: " << p);
  }
  else
  {
    vtkDebugLeaks::CriticalSection->Unlock();
  }
}
#else
void vtkDebugLeaks::DestructClass(const char*)
{
}
#endif

//----------------------------------------------------------------------------
void vtkDebugLeaks::SetDebugLeaksObserver(vtkDebugLeaksObserver* observer)
{
  vtkDebugLeaks::Observer = observer;
}

//----------------------------------------------------------------------------
vtkDebugLeaksObserver* vtkDebugLeaks::GetDebugLeaksObserver()
{
  return vtkDebugLeaks::Observer;
}

//----------------------------------------------------------------------------
void vtkDebugLeaks::ConstructingObject(vtkObjectBase* object)
{
  if (vtkDebugLeaks::Observer)
  {
    vtkDebugLeaks::Observer->ConstructingObject(object);
  }
}

//----------------------------------------------------------------------------
void vtkDebugLeaks::DestructingObject(vtkObjectBase* object)
{
  if (vtkDebugLeaks::Observer)
  {
    vtkDebugLeaks::Observer->DestructingObject(object);
  }
}

//----------------------------------------------------------------------------
int vtkDebugLeaks::PrintCurrentLeaks()
{
#ifdef VTK_DEBUG_LEAKS
  if(vtkDebugLeaks::MemoryTable->IsEmpty())
  {
    // Log something anyway, so users know vtkDebugLeaks is active/working.
    cerr << "vtkDebugLeaks has found no leaks.\n";
    return 0;
  }

  std::string leaks;
  std::string msg = "vtkDebugLeaks has detected LEAKS!\n";
  vtkDebugLeaks::MemoryTable->PrintTable(leaks);
  cerr << msg;
  cerr << leaks << endl << std::flush;

#ifdef _WIN32
  if(getenv("DASHBOARD_TEST_FROM_CTEST") ||
     getenv("DART_TEST_FROM_DART"))
  {
    // Skip dialogs when running on dashboard.
    return 1;
  }
  std::string::size_type myPos = 0;
  int cancel = 0;
  int count = 0;
  while(!cancel && myPos != leaks.npos)
  {
    std::string::size_type newPos = leaks.find('\n',myPos);
    if (newPos != leaks.npos)
    {
      msg += leaks.substr(myPos,newPos-myPos);
      msg += "\n";
      myPos = newPos;
      myPos++;
    }
    else
    {
      myPos = newPos;
    }
    count++;
    if (count == 10)
    {
      count = 0;
      cancel = vtkDebugLeaks::DisplayMessageBox(msg.c_str());
      msg = "";
    }
  }
  if (!cancel && count > 0)
  {
    vtkDebugLeaks::DisplayMessageBox(msg.c_str());
  }
#endif
#endif
  return 1;
}

//----------------------------------------------------------------------------
#ifdef _WIN32
int vtkDebugLeaks::DisplayMessageBox(const char* msg)
{
#ifdef UNICODE
  wchar_t *wmsg = new wchar_t [mbstowcs(nullptr, msg, 32000)+1];
  mbstowcs(wmsg, msg, 32000);
  int result = (MessageBox(nullptr, wmsg, L"Error",
                           MB_ICONERROR | MB_OKCANCEL) == IDCANCEL);
  delete [] wmsg;
#else
  int result = (MessageBox(nullptr, msg, "Error",
                           MB_ICONERROR | MB_OKCANCEL) == IDCANCEL);
#endif
  return result;
}
#else
int vtkDebugLeaks::DisplayMessageBox(const char*)
{
  return 0;
}
#endif

//----------------------------------------------------------------------------
int vtkDebugLeaks::GetExitError()
{
  return vtkDebugLeaks::ExitError;
}

//----------------------------------------------------------------------------
void vtkDebugLeaks::SetExitError(int flag)
{
  vtkDebugLeaks::ExitError = flag;
}

//----------------------------------------------------------------------------
void vtkDebugLeaks::ClassInitialize()
{
#ifdef VTK_DEBUG_LEAKS
  // Create the hash table.
  vtkDebugLeaks::MemoryTable = new vtkDebugLeaksHashTable;

  // Create the lock for the critical sections.
  vtkDebugLeaks::CriticalSection = new vtkSimpleCriticalSection;

  // Default to error when leaks occur while running tests.
  vtkDebugLeaks::ExitError = 1;
  vtkDebugLeaks::Observer = nullptr;
#else
  vtkDebugLeaks::MemoryTable = nullptr;
  vtkDebugLeaks::CriticalSection = nullptr;
  vtkDebugLeaks::ExitError = 0;
  vtkDebugLeaks::Observer = nullptr;
#endif
}

//----------------------------------------------------------------------------
void vtkDebugLeaks::ClassFinalize()
{
#ifdef VTK_DEBUG_LEAKS
  // Report leaks.
  int leaked = vtkDebugLeaks::PrintCurrentLeaks();

  // Destroy the hash table.
  delete vtkDebugLeaks::MemoryTable;
  vtkDebugLeaks::MemoryTable = nullptr;

  // Destroy the lock for the critical sections.
  delete vtkDebugLeaks::CriticalSection;
  vtkDebugLeaks::CriticalSection = nullptr;

  // Exit with error if leaks occurred and error mode is on.
  if(leaked && vtkDebugLeaks::ExitError)
  {
    exit(1);
  }
#endif
}

//----------------------------------------------------------------------------

// Purposely not initialized.  ClassInitialize will handle it.
vtkDebugLeaksHashTable* vtkDebugLeaks::MemoryTable;

// Purposely not initialized.  ClassInitialize will handle it.
vtkSimpleCriticalSection* vtkDebugLeaks::CriticalSection;

// Purposely not initialized.  ClassInitialize will handle it.
int vtkDebugLeaks::ExitError;

// Purposely not initialized.  ClassInitialize will handle it.
vtkDebugLeaksObserver* vtkDebugLeaks::Observer;
