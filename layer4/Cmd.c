/* 
A* -------------------------------------------------------------------
B* This file contains source code for the PyMOL computer program
C* copyright 1998-2000 by Warren Lyford Delano of DeLano Scientific. 
D* -------------------------------------------------------------------
E* It is unlawful to modify or remove this copyright notice.
F* -------------------------------------------------------------------
G* Please see the accompanying LICENSE file for further information. 
H* -------------------------------------------------------------------
I* Additional authors of this source file include:
-* 
-* 
-*
Z* -------------------------------------------------------------------
*/

/* 
   NOTICE:

   Important thread safety tip:

   PM operations which will ultimately call GLUT can only be called by
   the main GLUT thread (with some exceptions, such as the simple
   drawing operations which seem to be thread safe).

   Thus, pm.py needs to guard against direct invocation of certain _cmd
   (API) calls from outside threads [Examples: _cmd.png(..),
   _cmd.mpng(..) ].  Instead, it needs to hand them over to the main
   thread by way of a cmd.mdo(...)  statement.

   Note that current, most glut operations have been pushed into the
   main event and redraw loops to avoid these kinds of problems - so
   I'm not sure how important this really is anymore.

*/
   

/* TODO: Put in some exception handling and reporting for the
 * python calls, especially, PyArg_ParseTuple()
 */

#ifndef _PYMOL_NOPY

#include"os_predef.h"
#include"os_python.h"
#include"os_gl.h"
#include"os_std.h"
#include"Version.h"
#include"MemoryDebug.h"
#include"Err.h"
#include"Util.h"
#include"Cmd.h"
#include"ButMode.h"
#include"Ortho.h"
#include"ObjectMolecule.h"
#include"ObjectMesh.h"
#include"ObjectMap.h"
#include"ObjectCallback.h"
#include"ObjectCGO.h"
#include"ObjectSurface.h"
#include"ObjectSlice.h"
#include"Executive.h"
#include"Selector.h"
#include"main.h"
#include"Scene.h"
#include"Setting.h"
#include"Movie.h"
#include"Export.h"
#include"P.h"
#include"PConv.h"
#include"Control.h"
#include"Editor.h"
#include"Wizard.h"
#include"SculptCache.h"
#include"TestPyMOL.h"
#include"Color.h"
#include"Seq.h"
#include"PyMOL.h"
#include"Movie.h"
#include"OVContext.h"
#include"PlugIOManager.h"
#include"Seeker.h"

#define tmpSele "_tmp"
#define tmpSele1 "_tmp1"
#define tmpSele2 "_tmp2"

static int flush_count = 0;

#ifndef _PYMOL_NO_MAIN
#ifndef _PYMOL_WX_GLUT
static int run_only_once = true;
#endif
#endif
#ifdef _PYMOL_WX_GLUT
#ifndef _PYMOL_ACTIVEX
#ifndef _PYMOL_EMBEDDED
static int run_only_once = true;
#endif
#endif
#endif

int PyThread_get_thread_ident(void);

/* NOTE: the glut_thread_keep_out variable can only be changed by the thread
   holding the API lock, therefore this is safe even through increment
   isn't (necessarily) atomic. */

static void APIEntry(void) /* assumes API is locked */
{
  PRINTFD(TempPyMOLGlobals,FB_API)
    " APIEntry-DEBUG: as thread 0x%x.\n",PyThread_get_thread_ident()
    ENDFD;

  if(TempPyMOLGlobals->Terminating) {/* try to bail */
/* BEGIN PROPRIETARY CODE SEGMENT (see disclaimer in "os_proprietary.h") */ 
#ifdef WIN32
    abort();
#endif
/* END PROPRIETARY CODE SEGMENT */
    exit(0);
  }

  P_glut_thread_keep_out++;  
  PUnblock();
}

static void APIEnterBlocked(void) /* assumes API is locked */
{

  PRINTFD(TempPyMOLGlobals,FB_API)
    " APIEnterBlocked-DEBUG: as thread 0x%x.\n",PyThread_get_thread_ident()
    ENDFD;

  if(TempPyMOLGlobals->Terminating) {/* try to bail */
/* BEGIN PROPRIETARY CODE SEGMENT (see disclaimer in "os_proprietary.h") */ 
#ifdef WIN32
    abort();
#endif
/* END PROPRIETARY CODE SEGMENT */
    exit(0);
  }

  P_glut_thread_keep_out++;  
}

static void APIExit(void) /* assumes API is locked */
{
  PBlock();
  P_glut_thread_keep_out--;
  PRINTFD(TempPyMOLGlobals,FB_API)
    " APIExit-DEBUG: as thread 0x%x.\n",PyThread_get_thread_ident()
    ENDFD;
}

static void APIExitBlocked(void) /* assumes API is locked */
{

  P_glut_thread_keep_out--;
  PRINTFD(TempPyMOLGlobals,FB_API)
    " APIExitBlocked-DEBUG: as thread 0x%x.\n",PyThread_get_thread_ident()
    ENDFD;
}

static PyObject *APISuccess(void)/* success returns None */
{
  Py_INCREF(Py_None);
  return(Py_None);
}

static PyObject *APIFailure(void) /* returns -1: a general unspecified
                                   * error */
{
  return(Py_BuildValue("i",-1));
}

static PyObject *APIResultCode(int code) /* innteger result code
                                          * (could be a value, a
                                          * count, or a boolean) */
{
  return(Py_BuildValue("i",code));
}

static PyObject *APIResultOk(int ok) 
{
  if(ok)
    return APISuccess();
  else
    return APIFailure();
}

static PyObject *APIIncRef(PyObject *result) 
{
  Py_INCREF(result);
  return(result);
}
static PyObject *APIAutoNone(PyObject *result) /* automatically owned Py_None */
{
  if(result==Py_None)
    Py_INCREF(result);
  else if(result==NULL) {
    result=Py_None;
    Py_INCREF(result);
  } 
  return(result);
}

static PyObject *CmdSetSceneNames(PyObject *self, PyObject *args)
{
  PyObject *list;
  int ok = PyArg_ParseTuple(args,"O",&list);
  if(ok) {
    APIEnterBlocked();
    ok = SceneSetNames(TempPyMOLGlobals,list);
    APIExitBlocked();
  }
  return APIResultOk(ok);
}

static PyObject *CmdFixChemistry(PyObject *self, PyObject *args)
{
  char *str2,*str3;
  OrthoLineType s2="",s3="";
  int ok = false;
  int quiet;
  int invalidate;
  ok = PyArg_ParseTuple(args,"ssii",&str2,&str3,&invalidate,&quiet);
  if(ok) {
    APIEntry();
    ok = ((SelectorGetTmp(TempPyMOLGlobals,str2,s2)>=0) &&
          (SelectorGetTmp(TempPyMOLGlobals,str3,s3)>=0));
    if(ok) ok = ExecutiveFixChemistry(TempPyMOLGlobals,s2,s3,invalidate, quiet);
    SelectorFreeTmp(TempPyMOLGlobals,s3);
    SelectorFreeTmp(TempPyMOLGlobals,s2);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdGLDeleteLists(PyObject *self, PyObject *args)
{
  int int1,int2;
  int ok=false;
  ok = PyArg_ParseTuple(args,"ii",&int1,&int2);
  if(ok) {
    if(TempPyMOLGlobals->HaveGUI) {
      if(TempPyMOLGlobals->ValidContext) {
        glDeleteLists(int1,int2);
      }
    }
  }
  return APISuccess();
}
static PyObject *CmdRayAntiThread(PyObject *self, 	PyObject *args)
{
  int ok=true;
  PyObject *py_thread_info;

  CRayAntiThreadInfo *thread_info = NULL;

  ok = PyArg_ParseTuple(args,"O",&py_thread_info);
  if(ok) ok = PyCObject_Check(py_thread_info);
  if(ok) ok = ((thread_info = PyCObject_AsVoidPtr(py_thread_info))!=NULL);
  if (ok) {
    PUnblock();
    RayAntiThread(thread_info);
    PBlock();
  }
  return APIResultOk(ok);
}

static PyObject *CmdRayHashThread(PyObject *self, 	PyObject *args)
{
  int ok=true;
  PyObject *py_thread_info;

  CRayHashThreadInfo *thread_info = NULL;

  ok = PyArg_ParseTuple(args,"O",&py_thread_info);
  if(ok) ok = PyCObject_Check(py_thread_info);
  if(ok) ok = ((thread_info = PyCObject_AsVoidPtr(py_thread_info))!=NULL);
  if (ok) {
    PUnblock();
    RayHashThread(thread_info);
    PBlock();
  }
  return APIResultOk(ok);
}

static PyObject *CmdRayTraceThread(PyObject *self, 	PyObject *args)
{
  int ok=true;
  PyObject *py_thread_info;

  CRayThreadInfo *thread_info = NULL;

  ok = PyArg_ParseTuple(args,"O",&py_thread_info);
  if(ok) ok = PyCObject_Check(py_thread_info);
  if(ok) ok = ((thread_info = PyCObject_AsVoidPtr(py_thread_info))!=NULL);
  if (ok) {
    PUnblock();
    RayTraceThread(thread_info);
    PBlock();
  }
  return APIResultOk(ok);
}
static PyObject *CmdCoordSetUpdateThread(PyObject *self, 	PyObject *args)
{
  int ok=true;
  PyObject *py_thread_info;

  CCoordSetUpdateThreadInfo *thread_info = NULL;

  ok = PyArg_ParseTuple(args,"O",&py_thread_info);
  if(ok) ok = PyCObject_Check(py_thread_info);
  if(ok) ok = ((thread_info = PyCObject_AsVoidPtr(py_thread_info))!=NULL);
  if (ok) {
    PUnblock();
    CoordSetUpdateThread(thread_info);
    PBlock();
  }
  return APIResultOk(ok);
}

static PyObject *CmdObjectUpdateThread(PyObject *self, 	PyObject *args)
{
  int ok=true;
  PyObject *py_thread_info;

  CObjectUpdateThreadInfo *thread_info = NULL;

  ok = PyArg_ParseTuple(args,"O",&py_thread_info);
  if(ok) ok = PyCObject_Check(py_thread_info);
  if(ok) ok = ((thread_info = PyCObject_AsVoidPtr(py_thread_info))!=NULL);
  if (ok) {
    PUnblock();
    SceneObjectUpdateThread(thread_info);
    PBlock();
  }
  return APIResultOk(ok);
}

static PyObject *CmdGetMovieLocked(PyObject *self, 	PyObject *args)
{
  return APIResultCode(MovieLocked(TempPyMOLGlobals));
}

static PyObject *CmdFakeDrag(PyObject *self, 	PyObject *args)
{
  PyMOL_NeedFakeDrag(TempPyMOLGlobals->PyMOL);
  return APISuccess();
}

static PyObject *CmdDelColorection(PyObject *dummy, PyObject *args)
{
  int ok=true;
  PyObject *list;
  char *prefix;
  ok = PyArg_ParseTuple(args,"Os",&list,&prefix);
  if (ok) {
    APIEnterBlocked();
    ok = SelectorColorectionFree(TempPyMOLGlobals,list,prefix);
    APIExitBlocked();
  }
  return APIResultOk(ok);
}

static PyObject *CmdSetColorectionName(PyObject *dummy, PyObject *args)
{
  int ok=true;
  PyObject *list;
  char *prefix,*new_prefix;
  ok = PyArg_ParseTuple(args,"Oss",&list,&prefix,&new_prefix);
  if (ok) {
    APIEnterBlocked();
    ok = SelectorColorectionSetName(TempPyMOLGlobals,list,prefix,new_prefix);
    APIExitBlocked();
  }
  return APIResultOk(ok);
}

static PyObject *CmdSetColorection(PyObject *dummy, PyObject *args)
{
  int ok=true;
  char *prefix;
  PyObject *list;
  ok = PyArg_ParseTuple(args,"Os",&list,&prefix);
  if (ok) {
    APIEnterBlocked();
    ok = SelectorColorectionApply(TempPyMOLGlobals,list,prefix);
    APIExitBlocked();
  }
  return APIResultOk(ok);
}

static PyObject *CmdGetColorection(PyObject *dummy, PyObject *args)
{
  PyObject *result=NULL;
  int ok=true;
  char *prefix;
  ok = PyArg_ParseTuple(args,"s",&prefix);
  if (ok) {
    APIEnterBlocked();
    result = SelectorColorectionGet(TempPyMOLGlobals,prefix);
    APIExitBlocked();
  }
  return(APIAutoNone(result));
}

static PyObject *CmdGetRawAlignment(PyObject *dummy, PyObject *args)
{
  int ok=true;
  char *name;
  int active_only;
  PyObject *result = NULL;
  ok = PyArg_ParseTuple(args,"si",&name,&active_only);
  if(ok) {
    int align_sele = -1;
    APIEnterBlocked();
    if(name[0]) {
      CObject *obj = ExecutiveFindObjectByName(TempPyMOLGlobals,name);
      if(obj->type==cObjectAlignment) {
        align_sele = SelectorIndexByName(TempPyMOLGlobals,obj->Name);
      }
    } else {
      align_sele = ExecutiveGetActiveAlignmentSele(TempPyMOLGlobals);
    }
    if(align_sele>=0) {
      result = SeekerGetRawAlignment(TempPyMOLGlobals,align_sele,active_only);   
    }
    APIExitBlocked();
  }
  if(!result) {
    return APIFailure();
  } else 
    return result;
}

static PyObject *CmdGetOrigin(PyObject *dummy, PyObject *args)
{
  int ok=true;
  float origin[3];
  char *object;
  ok = PyArg_ParseTuple(args,"s",&object);
  if (ok) {
    APIEnterBlocked();
    if((!object)||(!object[0])) {
      SceneOriginGet(TempPyMOLGlobals,origin);
    } else {
      CObject *obj = ExecutiveFindObjectByName(TempPyMOLGlobals,object);
      if(!obj) {
        ok=false;
      } else {
        if(obj->TTTFlag) {
          origin[0] = -obj->TTT[12];
          origin[1] = -obj->TTT[13];
          origin[2] = -obj->TTT[14];
        } else {
          SceneOriginGet(TempPyMOLGlobals,origin); /* otherwise, return scene origin */
        }
      }
    }
    APIExitBlocked();
  }
  if(ok) {
    return(Py_BuildValue("fff",origin[0],origin[1],origin[2]));
  } else {
    return APIFailure();
  }
}

static PyObject *CmdGetVis(PyObject *dummy, PyObject *args)
{
  PyObject *result;
  int ok=true;
  if (ok) {
    APIEnterBlocked();
    result = ExecutiveGetVisAsPyDict(TempPyMOLGlobals);
    APIExitBlocked();
  }
  return(APIAutoNone(result));
}

static PyObject *CmdSetVis(PyObject *dummy, PyObject *args)
{
  int ok=true;
  PyObject *visDict;
  ok = PyArg_ParseTuple(args,"O",&visDict);
  if (ok) {
    APIEnterBlocked();
    ok = ExecutiveSetVisFromPyDict(TempPyMOLGlobals,visDict);
    APIExitBlocked();
  }
  return APIResultOk(ok);
}

static PyObject *CmdReinitialize(PyObject *dummy, PyObject *args)
{
  int ok=true;
  if (ok) {
    APIEntry();
    ok = ExecutiveReinitialize(TempPyMOLGlobals);
    APIExit();
  }
  return APIResultOk(ok);

}
static PyObject *CmdSpectrum(PyObject *self, PyObject *args)
{
  char *str1,*expr,*prefix;
  OrthoLineType s1;
  float min,max;
  int digits,start,stop, byres;
  int quiet;
  int ok=false;
  float min_ret,max_ret;
  PyObject *result = Py_None;
  ok = PyArg_ParseTuple(args,"ssffiisiii",&str1,&expr,
                        &min,&max,&start,&stop,&prefix,
                        &digits,&byres,&quiet);
  if (ok) {
    APIEntry();
    if(str1[0])
      ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    else
      s1[0]=0; /* no selection */
    if(ok) {
      ok = ExecutiveSpectrum(TempPyMOLGlobals,s1,expr,min,max,start,stop,prefix,digits,byres,quiet,
                             &min_ret,&max_ret);
    }
    if(str1[0])
      SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
    if(ok) {
      result = Py_BuildValue("ff",min_ret,max_ret);
    }
  }
  return(APIAutoNone(result));
}

static PyObject *CmdMDump(PyObject *self, PyObject *args)
{
  int ok=true;
  if(ok) {
    APIEntry();
    MovieDump(TempPyMOLGlobals);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdAccept(PyObject *self,PyObject *args)
{
  int ok=true;
  if(ok) {
    APIEntry();
    MovieSetLock(TempPyMOLGlobals,false);
    PRINTFB(TempPyMOLGlobals,FB_Movie,FB_Actions)
      " Movie: Risk accepted by user.  Movie commands have been enabled.\n"
      ENDFB(TempPyMOLGlobals);
    APIExit();
  }
  return APIResultOk(ok);
  
}

static PyObject *CmdDecline(PyObject *self,PyObject *args)
{
  int ok=true;
  if(ok) {
    APIEntry();
    MovieReset(TempPyMOLGlobals);
    PRINTFB(TempPyMOLGlobals,FB_Movie,FB_Actions)
      " Movie: Risk declined by user.  Movie commands have been deleted.\n"
      ENDFB(TempPyMOLGlobals);
    APIExit();
  }
  return APIResultOk(ok);
  
}

static PyObject *CmdSetCrystal(PyObject *self,PyObject *args)
{
  int ok=true;
  char *str1,*str2;
  OrthoLineType s1;
  float a,b,c,alpha,beta,gamma;

  ok = PyArg_ParseTuple(args,"sffffffs",&str1,&a,&b,&c,
                        &alpha,&beta,&gamma,&str2);
  if(ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    if(ok) ok = ExecutiveSetCrystal(TempPyMOLGlobals,s1,a,b,c,alpha,beta,gamma,str2);
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdGetCrystal(PyObject *self,PyObject *args)
{
  int ok=true;
  char *str1;    
  OrthoLineType s1;
  float a,b,c,alpha,beta,gamma;
  WordType sg;
  PyObject *result = NULL;
  int defined;
  ok = PyArg_ParseTuple(args,"s",&str1);
  if(ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    if(ok) ok = ExecutiveGetCrystal(TempPyMOLGlobals,s1,&a,&b,&c,&alpha,&beta,&gamma,sg,&defined);
    APIExit();
    if(ok) {
      if(defined) {
        result = PyList_New(7);
        if(result) {
          PyList_SetItem(result,0,PyFloat_FromDouble(a));
          PyList_SetItem(result,1,PyFloat_FromDouble(b));
          PyList_SetItem(result,2,PyFloat_FromDouble(c));
          PyList_SetItem(result,3,PyFloat_FromDouble(alpha));
          PyList_SetItem(result,4,PyFloat_FromDouble(beta));
          PyList_SetItem(result,5,PyFloat_FromDouble(gamma));
          PyList_SetItem(result,6,PyString_FromString(sg));
        }
      } else { /* no symmetry defined, then return empty list */
        result = PyList_New(0);
      }
    }
    SelectorFreeTmp(TempPyMOLGlobals,s1);
  }
  return(APIAutoNone(result));
}

static PyObject *CmdSmooth(PyObject *self,PyObject *args)
{
  int ok=true;
  char *str1;
  OrthoLineType s1;
  int int1,int2,int3,int4,int5,int6;
  ok = PyArg_ParseTuple(args,"siiiiii",&str1,&int1,&int2,&int3,&int4,&int5,&int6);
  if(ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    if(ok) ok = ExecutiveSmooth(TempPyMOLGlobals,s1,int1,int2,int3,int4,int5,int6);
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdGetSession(PyObject *self, PyObject *args)
{
  int ok=true;
  PyObject *dict;

  ok = PyArg_ParseTuple(args,"O",&dict);
  if(ok) {
    APIEntry();
    PBlock();
    ok = ExecutiveGetSession(TempPyMOLGlobals,dict);
    PUnblock();
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdSetSession(PyObject *self, PyObject *args)
{
  int ok=true;
  int quiet;
  PyObject *obj;

  ok = PyArg_ParseTuple(args,"Oi",&obj,&quiet);
  if(ok) {
    APIEntry();
    PBlock();
    ok = ExecutiveSetSession(TempPyMOLGlobals,obj,quiet);
    PUnblock();
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdSetName(PyObject *self, PyObject *args)
{
  int ok=true;
  char *str1,*str2;
  ok = PyArg_ParseTuple(args,"ss",&str1,&str2);
  if(ok) {
    APIEntry();
    ok = ExecutiveSetName(TempPyMOLGlobals,str1,str2);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdGetBondPrint(PyObject *self,PyObject *args)
{
  int ok=true;
  char *str1;
  int ***array = NULL;
  PyObject *result = NULL;
  int int1,int2;
  int dim[3];
  ok = PyArg_ParseTuple(args,"sii",&str1,&int1,&int2);
  if(ok) {
    APIEntry();
    array = ExecutiveGetBondPrint(TempPyMOLGlobals,str1,int1,int2,dim);
    APIExit();
    if(array) {
      result = PConv3DIntArrayTo3DPyList(array,dim);
      FreeP(array);
    }
  }
  return(APIAutoNone(result));
}

static PyObject *CmdDebug(PyObject *self,PyObject *args)
{
  int ok=true;
  char *str1;
  ok = PyArg_ParseTuple(args,"s",&str1);
  if(ok) {
    APIEntry();
    ok = ExecutiveDebug(TempPyMOLGlobals,str1);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdPGlutGetRedisplay(PyObject *self, PyObject *args)
{
#ifdef _PYMOL_PRETEND_GLUT
#ifndef _PYMOL_NO_GLUT
  return(APIResultCode(p_glutGetRedisplay()));  
#else
  return(APIResultCode(0));
#endif
#else
  return(APIResultCode(0));  
#endif
}

static PyObject *CmdPGlutEvent(PyObject *self, PyObject *args)
{
  int ok=true;
#ifdef _PYMOL_PRETEND_GLUT
#ifndef _PYMOL_NO_GLUT
  p_glut_event ev;
  ok = PyArg_ParseTuple(args,"iiiiii",&ev.event_code,
                        &ev.x,&ev.y,&ev.input,&ev.state,&ev.mod);
  if(ok) {
    PUnblock();
    p_glutHandleEvent(&ev);
    PBlock();
  }
#endif
#endif
  return APIResultOk(ok);
}

static PyObject *CmdSculptPurge(PyObject *self, PyObject *args)
{
  int ok=true;
  if(ok) {
    APIEntry();
    SculptCachePurge(TempPyMOLGlobals);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdSculptDeactivate(PyObject *self, PyObject *args)
{
  int ok=true;
  char *str1;
  ok = PyArg_ParseTuple(args,"s",&str1);
  if(ok) {
    APIEntry();
    ok = ExecutiveSculptDeactivate(TempPyMOLGlobals,str1);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdSculptActivate(PyObject *self, PyObject *args)
{
  int ok=true;
  int int1,int2,int3;
  char *str1;
  ok = PyArg_ParseTuple(args,"siii",&str1,&int1,&int2,&int3);
  if(ok) {
    APIEntry();
    ok = ExecutiveSculptActivate(TempPyMOLGlobals,str1,int1,int2,int3);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdSculptIterate(PyObject *self, PyObject *args)
{
  int ok=true;
  int int1,int2;
  char *str1;
  float total_strain = 0.0F;
  ok = PyArg_ParseTuple(args,"sii",&str1,&int1,&int2);
  if(ok) {
    APIEntry();
    total_strain = ExecutiveSculptIterate(TempPyMOLGlobals,str1,int1,int2);
    APIExit();
  }
  return(APIIncRef(PyFloat_FromDouble((double)total_strain)));
}

static PyObject *CmdSetObjectTTT(PyObject *self, 	PyObject *args)
{
  float ttt[16];
  int quiet;
  char *name;
  int state;
  int ok=PyArg_ParseTuple(args,"s(ffffffffffffffff)ii",
                          &name,
                          &ttt[ 0],&ttt[ 1],&ttt[ 2],&ttt[ 3], 
                          &ttt[ 4],&ttt[ 5],&ttt[ 6],&ttt[ 7],
                          &ttt[ 8],&ttt[ 9],&ttt[10],&ttt[11],
                          &ttt[12],&ttt[13],&ttt[14],&ttt[15],
                          &state,&quiet);
  if(ok) {
    APIEntry();
    ExecutiveSetObjectTTT(TempPyMOLGlobals,name,ttt,state,quiet);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdTranslateObjectTTT(PyObject *self, 	PyObject *args)
{
  float mov[3];
  char *name;
  int ok=PyArg_ParseTuple(args,"s(fff)",
                          &name,
                          &mov[0],&mov[1],&mov[2]);
  if(ok) {
    APIEntry();
    {
      CObject *obj = ExecutiveFindObjectByName(TempPyMOLGlobals,name);
      if(obj) {
        ObjectTranslateTTT(obj,mov); 
        SceneInvalidate(TempPyMOLGlobals);
      } else
        ok=false;
    }
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdCombineObjectTTT(PyObject *self, 	PyObject *args)
{
  char *name;
  PyObject *m;
  float ttt[16];
  int ok = false;
  ok = PyArg_ParseTuple(args,"sO",&name,&m);
  if(ok) {
    if(PConvPyListToFloatArrayInPlace(m,ttt,16)>0) {
      APIEntry();
      ok = ExecutiveCombineObjectTTT(TempPyMOLGlobals,name,ttt,false);
      APIExit();
    } else {
      PRINTFB(TempPyMOLGlobals,FB_CCmd,FB_Errors)
        "CmdCombineObjectTTT-Error: bad matrix\n"
        ENDFB(TempPyMOLGlobals);
      ok=false;
    }
  }
  return APIResultOk(ok);
}

static PyObject *CmdGetColor(PyObject *self, PyObject *args)
{
  char *name;
  int mode;
  int ok = false;
  int a,nc,nvc;
  float *rgb;
  int index;
  PyObject *result = NULL;
  PyObject *tup;
  ok = PyArg_ParseTuple(args,"si",&name,&mode);
  if(ok) {
    APIEnterBlocked();
    switch(mode) {
    case 0: /* by name or index, return floats */
      index = ColorGetIndex(TempPyMOLGlobals,name);
      if(index>=0) {
        rgb = ColorGet(TempPyMOLGlobals,index);
        tup = PyTuple_New(3);
        PyTuple_SetItem(tup,0,PyFloat_FromDouble(*(rgb++)));
        PyTuple_SetItem(tup,1,PyFloat_FromDouble(*(rgb++)));
        PyTuple_SetItem(tup,2,PyFloat_FromDouble(*rgb));
        result=tup;
      }
      break;
    case 1: /* get color names with NO NUMBERS in their names */
      nc=ColorGetNColor(TempPyMOLGlobals);
      nvc=0;
      for(a=0;a<nc;a++) {
        if(ColorGetStatus(TempPyMOLGlobals,a)==1)
          nvc++;
      }
      result = PyList_New(nvc);
      nvc=0;
      for(a=0;a<nc;a++) {
        if(ColorGetStatus(TempPyMOLGlobals,a)==1) {
          tup = PyTuple_New(2);
          PyTuple_SetItem(tup,0,PyString_FromString(ColorGetName(TempPyMOLGlobals,a)));
          PyTuple_SetItem(tup,1,PyInt_FromLong(a));
          PyList_SetItem(result,nvc++,tup);
        }
      }
      break;
    case 2: /* get all colors */
      nc=ColorGetNColor(TempPyMOLGlobals);
      nvc=0;
      for(a=0;a<nc;a++) {
        if(ColorGetStatus(TempPyMOLGlobals,a)!=0)
          nvc++;
      }
      result = PyList_New(nvc);
      nvc=0;
      for(a=0;a<nc;a++) {
        if(ColorGetStatus(TempPyMOLGlobals,a)) {
          tup = PyTuple_New(2);
          PyTuple_SetItem(tup,0,PyString_FromString(ColorGetName(TempPyMOLGlobals,a)));
          PyTuple_SetItem(tup,1,PyInt_FromLong(a));
          PyList_SetItem(result,nvc++,tup);
        }
      }
      break;
    case 3: /* get a single color index */
      result = PyInt_FromLong(ColorGetIndex(TempPyMOLGlobals,name));
      break;
    case 4: /* by name or index, return floats including negative R for special colors */
      index = ColorGetIndex(TempPyMOLGlobals,name);
      rgb = ColorGetSpecial(TempPyMOLGlobals,index);
      tup = PyTuple_New(3);
      PyTuple_SetItem(tup,0,PyFloat_FromDouble(*(rgb++)));
      PyTuple_SetItem(tup,1,PyFloat_FromDouble(*(rgb++)));
      PyTuple_SetItem(tup,2,PyFloat_FromDouble(*rgb));
      result=tup;
      break;
    }
    APIExitBlocked();
  }
  return(APIAutoNone(result));
}

static PyObject *CmdGetChains(PyObject *self, PyObject *args)
{

  char *str1;
  int int1;
  OrthoLineType s1="";
  PyObject *result = NULL;
  char *chain_str = NULL;
  int ok=false;
  int c1=0;
  int a,l;
  int null_chain = false;
  ok = PyArg_ParseTuple(args,"si",&str1,&int1);
  if(ok) {
    APIEntry();
    if(str1[0]) c1 = SelectorGetTmp(TempPyMOLGlobals,str1,s1);
    if(c1>=0)
      chain_str = ExecutiveGetChains(TempPyMOLGlobals,s1,int1,&null_chain);
    APIExit();
    if(chain_str) {
      l=strlen(chain_str);
      if(null_chain) l++; 
      result = PyList_New(l);
      if(null_chain) {
        l--;
        PyList_SetItem(result,l,PyString_FromString(""));
      }
      for(a=0;a<l;a++)
        PyList_SetItem(result,a,PyString_FromStringAndSize(chain_str+a,1));

    }
    FreeP(chain_str);
    if(s1[0]) SelectorFreeTmp(TempPyMOLGlobals,s1);
  }
  if(result) {
    return(APIAutoNone(result));
  } else {
    return APIFailure();
  }
}

static PyObject *CmdMultiSave(PyObject *self, PyObject *args)
{
  char *name,*object;
  int append,state;
  int ok = false;
  ok = PyArg_ParseTuple(args,"ssii",&name,&object,&state,&append);
  if(ok) {
    APIEntry();
    ok = ExecutiveMultiSave(TempPyMOLGlobals,name,object,state,append);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdRampNew(PyObject *self, 	PyObject *args)
{
  char *name;
  int ok = false;
  char *map;
  int state;
  char *sele;
  float beyond,within;
  float sigma;
  int zero;
  OrthoLineType s1;
  PyObject *range,*color;
  ok = PyArg_ParseTuple(args,"ssOOisfffi",&name,&map,&range,&color,
                        &state,&sele,&beyond,&within,
                        &sigma,&zero);
  if(ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,sele,s1)>=0);
    if(ok) ok = ExecutiveRampNew(TempPyMOLGlobals,name,map,range,color,state,s1,beyond,within,sigma,zero);
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdMapNew(PyObject *self, PyObject *args)
{
  char *name;
  float minCorner[3],maxCorner[3];
  float grid[3];
  float buffer;
  int type;
  int state;
  int have_corners;
  int quiet,zoom;
  char *selection;
  OrthoLineType s1 = "";
  int ok = false;
  ok = PyArg_ParseTuple(args,"sifsf(ffffff)iiii",&name,&type,&grid[0],&selection,&buffer,
                        &minCorner[0],&minCorner[1],&minCorner[2],
                        &maxCorner[0],&maxCorner[1],&maxCorner[2],
                        &state,&have_corners,&quiet,&zoom);
  if(ok) {
    APIEntry();
    grid[1]=grid[0];
    grid[2]=grid[0];
    ok = (SelectorGetTmp(TempPyMOLGlobals,selection,s1)>=0);
    if(ok) ok = ExecutiveMapNew(TempPyMOLGlobals,name,type,grid,s1,buffer,
                         minCorner,maxCorner,state,have_corners,quiet,zoom);
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();

  }
  return APIResultOk(ok);
}

static PyObject *CmdMapSetBorder(PyObject *self, PyObject *args)
{
  char *name;
  float level;
  int state;
  int ok = false;
  ok = PyArg_ParseTuple(args,"sfi",&name,&level,&state);
  if(ok) {
    APIEntry();
    ok = ExecutiveMapSetBorder(TempPyMOLGlobals,name,level,state);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdMapTrim(PyObject *self, PyObject *args)
{
  char *name,*sele;
  int map_state,sele_state;
  int ok = false;
  float buffer;
  int quiet;
  OrthoLineType s1;
  ok = PyArg_ParseTuple(args,"ssfiii",&name,&sele,&buffer,
                        &map_state,&sele_state,&quiet);
  if(ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,sele,s1)>=0);
    ok = ExecutiveMapTrim(TempPyMOLGlobals,name,s1,buffer,map_state,sele_state,quiet);
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdMapDouble(PyObject *self, PyObject *args)
{
  char *name;
  int state;
  int ok = false;
  ok = PyArg_ParseTuple(args,"si",&name,&state);
  if(ok) {
    APIEntry();
    ok = ExecutiveMapDouble(TempPyMOLGlobals,name,state);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdMapHalve(PyObject *self, PyObject *args)
{
  char *name;
  int state;
  int ok = false;
  int smooth;
  ok = PyArg_ParseTuple(args,"sii",&name,&state,&smooth);
  if(ok) {
    APIEntry();
    ok = ExecutiveMapHalve(TempPyMOLGlobals,name,state,smooth);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdGetRenderer(PyObject *self, PyObject *args)
{
  char *vendor,*renderer,*version;
  APIEntry();
  SceneGetCardInfo(TempPyMOLGlobals,&vendor,&renderer,&version);
  APIExit();
  return Py_BuildValue("(sss)",vendor,renderer,version);
}

static PyObject *CmdGetVersion(PyObject *self, PyObject *args)
{
  double ver_num = _PyMOL_VERSION_double;
  WordType ver_str;
  strcpy(ver_str,_PyMOL_VERSION);
  return Py_BuildValue("(sd)",ver_str, ver_num);
}

static PyObject *CmdTranslateAtom(PyObject *self, PyObject *args)
{
  char *str1;
  int state,log,mode;
  float v[3];
  OrthoLineType s1;
  int ok = false;
  ok = PyArg_ParseTuple(args,"sfffiii",&str1,v,v+1,v+2,&state,&mode,&log);
  if(ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    if(ok) ok = ExecutiveTranslateAtom(TempPyMOLGlobals,s1,v,state,mode,log);
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdMatrixTransfer(PyObject *self, 	PyObject *args)
{
  char *source_name, *target_name;
  int source_mode, target_mode;
  int source_state, target_state, target_undo;
  int log;
  int quiet;
  int ok=false;
  ok = PyArg_ParseTuple(args,"ssiiiiiii",
                        &source_name, &target_name,
                        &source_mode, &target_mode,
                        &source_state, &target_state,
                        &target_undo,
                        &log, &quiet);
  if (ok) {
    APIEntry();
    ExecutiveMatrixTransfer(TempPyMOLGlobals,
                            source_name, target_name, 
                            source_mode, target_mode, 
                            source_state, target_state,
                            target_undo,
                            log, quiet);
    APIExit();
  }
  return APIResultOk(ok);  
}

static PyObject *CmdResetMatrix(PyObject *self, 	PyObject *args)
{
  char *name;
  int mode;
  int state;
  int log;
  int quiet;
  int ok=false;
  ok = PyArg_ParseTuple(args,"siiii",
                        &name,
                        &mode,
                        &state,
                        &log, &quiet);
  if (ok) {
    APIEntry();
    ExecutiveResetMatrix(TempPyMOLGlobals,
                         name,
                         mode,
                         state,
                         log, quiet);
    APIExit();
  }
  return APIResultOk(ok);  
}

static PyObject *CmdTransformObject(PyObject *self, PyObject *args)
{
  char *name,*sele;
  int state,log;
  PyObject *m;
  float matrix[16];
  int homo;
  int ok = false;
  ok = PyArg_ParseTuple(args,"siOisi",&name,&state,&m,&log,&sele,&homo);
  if(ok) {
    if(PConvPyListToFloatArrayInPlace(m,matrix,16)>0) {
      APIEntry();
      ok = ExecutiveTransformObjectSelection(TempPyMOLGlobals,name,
                                             state,sele,log,matrix,
                                             homo);
      APIExit();
    } else {
      PRINTFB(TempPyMOLGlobals,FB_CCmd,FB_Errors)
        "CmdTransformObject-DEBUG: bad matrix\n"
        ENDFB(TempPyMOLGlobals);
      ok=false;
    }
  }
  return APIResultOk(ok);
}

static PyObject *CmdTransformSelection(PyObject *self, PyObject *args)
{
  char *sele;
  int state,log;
  int homo;
  PyObject *m;
  float ttt[16];
  OrthoLineType s1;
  int ok = false;
  ok = PyArg_ParseTuple(args,"siOii",&sele,&state,&m,&log,&homo);
  if(ok) {
    APIEntry();
    if(PConvPyListToFloatArrayInPlace(m,ttt,16)>0) {
      ok = (SelectorGetTmp(TempPyMOLGlobals,sele,s1)>=0);
      if(ok) ok = ExecutiveTransformSelection(TempPyMOLGlobals,state,s1,log,ttt,homo);
      SelectorFreeTmp(TempPyMOLGlobals,s1);
    } else {
      PRINTFB(TempPyMOLGlobals,FB_CCmd,FB_Errors)
        "CmdTransformSelection-DEBUG: bad matrix\n"
        ENDFB(TempPyMOLGlobals);
      ok=false;
    }
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdLoadColorTable(PyObject *self, PyObject *args)
{
  char *str1;
  int ok = false;
  int quiet;
  ok = PyArg_ParseTuple(args,"si",&str1,&quiet);
  if(ok) {
    APIEntry();
    ok = ColorTableLoad(TempPyMOLGlobals,str1,quiet);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdLoadPNG(PyObject *self, PyObject *args)
{
  char *str1;
  int ok = false;
  int quiet;
  int movie,stereo;
  ok = PyArg_ParseTuple(args,"siii",&str1,&movie,&stereo,&quiet);
  if(ok) {
    APIEntry();
    ok = SceneLoadPNG(TempPyMOLGlobals,str1,movie,stereo,quiet);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdBackgroundColor(PyObject *self, PyObject *args)
{
  char *str1;
  int ok = false;
  int idx;
  ok = PyArg_ParseTuple(args,"s",&str1);
  if(ok) {
    APIEntry();
    idx = ColorGetIndex(TempPyMOLGlobals,str1);
    if(idx>=0)
      ok = SettingSetfv(TempPyMOLGlobals,cSetting_bg_rgb,ColorGet(TempPyMOLGlobals,idx));
    else {
      ErrMessage(TempPyMOLGlobals,"Color","Bad color name.");
      ok = false; /* bad color */
    }
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdGetPosition(PyObject *self, 	PyObject *args)
{
  PyObject *result;
  float v[3];
  APIEntry();
  SceneGetPos(TempPyMOLGlobals,v);
  APIExit();
  result=PConvFloatArrayToPyList(v,3);
  return(result);
}

static PyObject *CmdGetMoviePlaying(PyObject *self, 	PyObject *args)
{
  PyObject *result;
  result=PyInt_FromLong(MoviePlaying(TempPyMOLGlobals));
  return(result);
}


static PyObject *CmdGetPhiPsi(PyObject *self, 	PyObject *args)
{
  char *str1;
  OrthoLineType s1;
  int state;
  PyObject *result = Py_None;
  PyObject *key = Py_None;
  PyObject *value = Py_None;
  int *iVLA=NULL;
  float *pVLA=NULL,*sVLA=NULL;
  int l = 0;
  int *i;
  ObjectMolecule **o,**oVLA=NULL;
  int a;
  float *s,*p;
  int ok =  PyArg_ParseTuple(args,"si",&str1,&state);
  if(ok) {
    APIEntry();
    ok=(SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    if(ok) 
      l = ExecutivePhiPsi(TempPyMOLGlobals,s1,&oVLA,&iVLA,&pVLA,&sVLA,state);
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
    if(iVLA) {
      result=PyDict_New();
      i = iVLA;
      o = oVLA;
      p = pVLA;
      s = sVLA;
      for(a=0;a<l;a++) {
        key = PyTuple_New(2);      
        PyTuple_SetItem(key,1,PyInt_FromLong(*(i++)+1)); /* +1 for index */
        PyTuple_SetItem(key,0,PyString_FromString((*(o++))->Obj.Name));
        value = PyTuple_New(2);      
        PyTuple_SetItem(value,0,PyFloat_FromDouble(*(p++))); /* +1 for index */
        PyTuple_SetItem(value,1,PyFloat_FromDouble(*(s++)));
        PyDict_SetItem(result,key,value);
        Py_DECREF(key);
        Py_DECREF(value);
      }
    } else {
      result = PyDict_New();
    }
    VLAFreeP(iVLA);
    VLAFreeP(oVLA);
    VLAFreeP(sVLA);
    VLAFreeP(pVLA);
  }
  return(APIAutoNone(result));
}

static PyObject *CmdAlign(PyObject *self, 	PyObject *args) {
  char *str2,*str3,*mfile,*oname;
  OrthoLineType s2="",s3="";
  float result = -1.0;
  int ok = false;
  int quiet,cycles,max_skip;
  float cutoff,gap,extend;
  int state1,state2;
  int max_gap;
  ExecutiveRMSInfo rms_info;

  ok = PyArg_ParseTuple(args,"ssfiffissiiii",&str2,&str3,
                        &cutoff,&cycles,&gap,&extend,&max_gap,&oname,
                        &mfile,&state1,&state2,&quiet,&max_skip);

  if(ok) {
    PRINTFD(TempPyMOLGlobals,FB_CCmd)
      "CmdAlign-DEBUG %s %s\n",
      str2,str3
      ENDFD;
    APIEntry();
    
    ok = ((SelectorGetTmp(TempPyMOLGlobals,str2,s2)>=0) &&
          (SelectorGetTmp(TempPyMOLGlobals,str3,s3)>=0));
    if(ok) {
      ExecutiveAlign(TempPyMOLGlobals,s2,s3,
                     mfile,gap,extend,max_gap,
                     max_skip,cutoff,
                     cycles,quiet,oname,state1,state2,
                     &rms_info);
    } else 
      result = -1.0F;
    SelectorFreeTmp(TempPyMOLGlobals,s2);
    SelectorFreeTmp(TempPyMOLGlobals,s3);
    APIExit();
  }
  if(ok) {
    return Py_BuildValue("(fiififi)",
                         rms_info.final_rms,
                         rms_info.final_n_atom,
                         rms_info.n_cycles_run,
                         rms_info.initial_rms,
                         rms_info.initial_n_atom,
                         rms_info.raw_alignment_score,
                         rms_info.n_residues_aligned);
  } else {
    return APIFailure();
  }
}


static PyObject *CmdGetSettingUpdates(PyObject *self, 	PyObject *args)
{
  PyObject *result = NULL;
  APIEnterBlocked();
  result = SettingGetUpdateList(TempPyMOLGlobals,NULL);
  APIExitBlocked();
  return(APIAutoNone(result));
}

static PyObject *CmdGetView(PyObject *self, 	PyObject *args)
{
  SceneViewType view;
  APIEntry();
  SceneGetView(TempPyMOLGlobals,view);
  APIExit();
  return(Py_BuildValue("(fffffffffffffffffffffffff)",
                   view[ 0],view[ 1],view[ 2],view[ 3], /* 4x4 mat */
                   view[ 4],view[ 5],view[ 6],view[ 7],
                   view[ 8],view[ 9],view[10],view[11],
                   view[12],view[13],view[14],view[15],
                   view[16],view[17],view[18], /* pos */
                   view[19],view[20],view[21], /* origin */
                   view[22],view[23], /* clip */
                   view[24] /* orthoscopic*/
                       ));
}
static PyObject *CmdSetView(PyObject *self, 	PyObject *args)
{
  SceneViewType view;
  int quiet;
  float animate;
  int hand;
  int ok=PyArg_ParseTuple(args,"(fffffffffffffffffffffffff)ifi",
                   &view[ 0],&view[ 1],&view[ 2],&view[ 3], /* 4x4 mat */
                   &view[ 4],&view[ 5],&view[ 6],&view[ 7],
                   &view[ 8],&view[ 9],&view[10],&view[11],
                   &view[12],&view[13],&view[14],&view[15],
                   &view[16],&view[17],&view[18], /* pos */
                   &view[19],&view[20],&view[21], /* origin */
                   &view[22],&view[23], /* clip */
                   &view[24], /* orthoscopic*/
                   &quiet,&animate,&hand);
  if(ok) {
    APIEntry();
    SceneSetView(TempPyMOLGlobals,view,quiet,animate,hand); /* TODO STATUS */
    APIExit();
  }
  return APIResultOk(ok);
}
static PyObject *CmdGetState(PyObject *self, 	PyObject *args)
{
  return(APIResultCode(SceneGetState(TempPyMOLGlobals)));
}
static PyObject *CmdGetEditorScheme(PyObject *self, 	PyObject *args)
{
  return(APIResultCode(EditorGetScheme(TempPyMOLGlobals)));
}

static PyObject *CmdGetFrame(PyObject *self, 	PyObject *args)
{
  return(APIResultCode(SceneGetFrame(TempPyMOLGlobals)+1));
}

static PyObject *CmdSetTitle(PyObject *self, PyObject *args)
{
  char *str1,*str2;
  int int1;
  int ok = false;
  ok = PyArg_ParseTuple(args,"sis",&str1,&int1,&str2);
  if(ok) {
    APIEntry();
    ok = ExecutiveSetTitle(TempPyMOLGlobals,str1,int1,str2);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdGetTitle(PyObject *self, PyObject *args)
{
  char *str1,*str2;
  int int1;
  int ok = false;
  PyObject *result = Py_None;
  ok = PyArg_ParseTuple(args,"si",&str1,&int1);
  if(ok) {
    APIEntry();
    str2 = ExecutiveGetTitle(TempPyMOLGlobals,str1,int1);
    APIExit();
    if(str2) result = PyString_FromString(str2);
  }
  return(APIAutoNone(result));
}


static PyObject *CmdExportCoords(PyObject *self, 	PyObject *args)
{
  void *result;
  char *str1;
  int int1;
  PyObject *py_result = Py_None;
  int ok = false;
  ok = PyArg_ParseTuple(args,"si",&str1,&int1);
  if(ok) {
    APIEntry();
    result = ExportCoordsExport(TempPyMOLGlobals,str1,int1,0);
    APIExit();
    if(result) 
      py_result = PyCObject_FromVoidPtr(result,(void(*)(void*))ExportCoordsFree);
  }
  return(APIAutoNone(py_result));
}

static PyObject *CmdImportCoords(PyObject *self, 	PyObject *args)
{
  char *str1;
  int int1;
  PyObject *cObj;
  void *mmdat=NULL;
  int ok = false;
  ok = PyArg_ParseTuple(args,"siO",&str1,&int1,&cObj);
  if(ok) {
    if(PyCObject_Check(cObj))
      mmdat = PyCObject_AsVoidPtr(cObj);
    APIEntry();
    if(mmdat)
      ok = ExportCoordsImport(TempPyMOLGlobals,str1,int1,mmdat,0);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdGetArea(PyObject *self, 	PyObject *args)
{
  char *str1;
  int int1,int2;
  OrthoLineType s1="";
  float result = -1.0;
  int ok=false;
  int c1=0;
  ok = PyArg_ParseTuple(args,"sii",&str1,&int1,&int2);
  if(ok) {
    APIEntry();
    if(str1[0]) c1 = SelectorGetTmp(TempPyMOLGlobals,str1,s1);
    if(c1>=0)
      result = ExecutiveGetArea(TempPyMOLGlobals,s1,int1,int2);
    else
      result = -1.0F;
    if(s1[0]) SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();

  }
  return(Py_BuildValue("f",result));

}

static PyObject *CmdPushUndo(PyObject *self, 	PyObject *args)
{
  char *str0;
  int state;
  OrthoLineType s0="";
  int ok=true;
  ok = PyArg_ParseTuple(args,"si",&str0,&state);
  if(ok) {
    APIEntry();
    if(str0[0]) ok = (SelectorGetTmp(TempPyMOLGlobals,str0,s0)>=0);
    if(ok) ok = ExecutiveSaveUndo(TempPyMOLGlobals,s0,state);
    if(s0[0]) SelectorFreeTmp(TempPyMOLGlobals,s0);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdGetType(PyObject *self, 	PyObject *args)
{
  char *str1;
  WordType type = "";
  int ok=false;
  ok = PyArg_ParseTuple(args,"s",&str1);
  if (ok) {
    APIEntry();
    ok = ExecutiveGetType(TempPyMOLGlobals,str1,type);
    APIExit();
  } 
  if(ok) 
    return(Py_BuildValue("s",type));
  else
    return APIResultOk(ok);
}
static PyObject *CmdGetNames(PyObject *self, 	PyObject *args)
{
  int int1,int2;
  char *vla = NULL;
  OrthoLineType s0="";
  PyObject *result = Py_None;
  int ok=false;
  char *str0;
  ok = PyArg_ParseTuple(args,"iis",&int1,&int2,&str0);
  if(ok) {
    APIEntry();
    if(str0[0]) ok = (SelectorGetTmp(TempPyMOLGlobals,str0,s0)>=0);
    vla = ExecutiveGetNames(TempPyMOLGlobals,int1,int2,s0);
    if(s0[0]) SelectorFreeTmp(TempPyMOLGlobals,s0);
    APIExit();
    result = PConvStringVLAToPyList(vla);
    VLAFreeP(vla);
  }
  return(APIAutoNone(result));
}

static PyObject *CmdInterrupt(PyObject *self, PyObject *args)
{
  int int1;
  int ok=false;
  ok = PyArg_ParseTuple(args,"i",&int1);
  if(ok) {
    PyMOL_SetInterrupt(TempPyMOLGlobals->PyMOL, int1);
  }
  return APIResultOk(ok);
}

static PyObject *CmdInvert(PyObject *self, PyObject *args)
{
  int int1;
  int ok=false;
  ok = PyArg_ParseTuple(args,"i",&int1);
  if(ok) {
    APIEntry();
    ok = ExecutiveInvert(TempPyMOLGlobals,int1);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdTorsion(PyObject *self, PyObject *args)
{
  float float1;
  int ok=false;
  ok = PyArg_ParseTuple(args,"f",&float1);
  if (ok) {
    APIEntry();
    ok = EditorTorsion(TempPyMOLGlobals,float1);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdUndo(PyObject *self, PyObject *args)
{
  int int1;
  int ok=false;
  ok = PyArg_ParseTuple(args,"i",&int1);
  if (ok) {
    APIEntry();
    ExecutiveUndo(TempPyMOLGlobals,int1); /* TODO STATUS */
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdMask(PyObject *self, PyObject *args)
{
  char *str1;
  int int1;
  OrthoLineType s1;

  int ok=false;
  ok = PyArg_ParseTuple(args,"si",&str1,&int1);
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    ExecutiveMask(TempPyMOLGlobals,s1,int1); /* TODO STATUS */
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdProtect(PyObject *self, PyObject *args)
{
  char *str1;
  int int1,int2;
  OrthoLineType s1;

  int ok=false;
  ok = PyArg_ParseTuple(args,"sii",&str1,&int1,&int2);
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    ExecutiveProtect(TempPyMOLGlobals,s1,int1,int2); /* TODO STATUS */
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();

  }
  return APIResultOk(ok);
}


static PyObject *CmdButton(PyObject *self, 	PyObject *args)
{
  int i1,i2;
  int ok=false;
  ok = PyArg_ParseTuple(args,"ii",&i1,&i2);
  if (ok) {
    APIEntry();
    ButModeSet(TempPyMOLGlobals,i1,i2); /* TODO STATUS */
    APIExit();
  }
  return APIResultOk(ok);  
}

static PyObject *CmdFeedback(PyObject *self, 	PyObject *args)
{
  int i1,i2,result = 0;
  int ok=false;
  ok = PyArg_ParseTuple(args,"ii",&i1,&i2);
  if (ok) {
    /* NO API Entry for performance,
     *feedback (MACRO) just accesses a safe global */
    result = Feedback(TempPyMOLGlobals,i1,i2);
  }
  return Py_BuildValue("i",result);
}

static PyObject *CmdSetFeedbackMask(PyObject *self, 	PyObject *args)
{
  int i1,i2,i3;
  int ok=false;
  ok = PyArg_ParseTuple(args,"iii",&i1,&i2,&i3);
  if (ok) {
    APIEntry();
    switch(i1) { /* TODO STATUS */
    case 0: 
      FeedbackSetMask(TempPyMOLGlobals,i2,(uchar)i3);
      break;
    case 1:
      FeedbackEnable(TempPyMOLGlobals,i2,(uchar)i3);
      break;
    case 2:
      FeedbackDisable(TempPyMOLGlobals,i2,(uchar)i3);
      break;
    case 3:
      FeedbackPush(TempPyMOLGlobals);
      break;
    case 4:
      FeedbackPop(TempPyMOLGlobals);
      break;
    }
    APIExit();
  }
  return APIResultOk(ok);  
}

static PyObject *CmdPop(PyObject *self,  PyObject *args)
{
  char *str1,*str2;
  int quiet;
  int result = 0;
  int ok=true;
  ok = PyArg_ParseTuple(args,"ssi",&str1,&str2,&quiet);
  if (ok) {
    APIEntry();
    result = ExecutivePop(TempPyMOLGlobals,str1,str2,quiet); 
    APIExit();
  } else
    result = -1;
  return(APIResultCode(result));

}


static PyObject *CmdFlushNow(PyObject *self, 	PyObject *args)
{
  if(TempPyMOLGlobals && TempPyMOLGlobals->Ready) {
    /* only called by the GLUT thread with unlocked API, blocked interpreter */
    if(flush_count<8) { /* prevent super-deep recursion */
      flush_count++;
      PFlushFast();
      flush_count--;
    } else {
      PRINTFB(TempPyMOLGlobals,FB_CCmd,FB_Warnings)
        " Cmd: PyMOL lagging behind API requests...\n"
        ENDFB(TempPyMOLGlobals);
    }
  }
  return APISuccess();  
}

static PyObject *CmdWaitQueue(PyObject *self, 	PyObject *args)
{
  /* called by non-GLUT thread with unlocked API, blocked interpreter */
  PyObject *result = NULL;
  if(!TempPyMOLGlobals->Terminating) {
    APIEnterBlocked();
    if(OrthoCommandWaiting(TempPyMOLGlobals)
       ||(flush_count>1))
      result = PyInt_FromLong(1);
    else
      result = PyInt_FromLong(0);
    APIExitBlocked();
  }
  return APIAutoNone(result);
}

static PyObject *CmdWaitDeferred(PyObject *self, 	PyObject *args)
{
  PyObject *result = NULL;
  if(!TempPyMOLGlobals->Terminating) {
    APIEnterBlocked();
    if(OrthoDeferredWaiting(TempPyMOLGlobals))
      result = PyInt_FromLong(1);
    else
      result = PyInt_FromLong(0);
    APIExitBlocked();
  }
  return APIAutoNone(result);
}

static PyObject *CmdPaste(PyObject *dummy, PyObject *args)
{
  PyObject *list,*str;
  char *st;
  int l,a;
  int ok=false;
  ok = PyArg_ParseTuple(args,"O",&list);
  if(ok) {
    if(!list) 
      ok=false;
    else if(!PyList_Check(list)) 
      ok=false;
    else
      {
        l=PyList_Size(list);
        for(a=0;a<l;a++) {
          str = PyList_GetItem(list,a);
          if(str) {
            if(PyString_Check(str)) {
              st = PyString_AsString(str);
              APIEntry();
              OrthoPasteIn(TempPyMOLGlobals,st);
              if(a<(l-1))
                OrthoPasteIn(TempPyMOLGlobals,"\n");
              APIExit();
            } else {
              ok = false;
            }
          }
        }
      }
  }
  return APIResultOk(ok);  
}

static PyObject *CmdGetVRML(PyObject *dummy, PyObject *args)
{
  PyObject *result = NULL;
  char *vla = NULL;
  APIEntry();
  SceneRay(TempPyMOLGlobals,0,0,4,NULL,
           &vla,0.0F,0.0F,false,NULL,false,-1);
  APIExit();
  if(vla) {
    result = Py_BuildValue("s",vla);
  }
  VLAFreeP(vla);
  return(APIAutoNone(result));
}

static PyObject *CmdGetPovRay(PyObject *dummy, PyObject *args)
{
  PyObject *result = NULL;
  char *header=NULL,*geom=NULL;
  APIEntry();
  SceneRay(TempPyMOLGlobals,0,0,1,&header,
           &geom,0.0F,0.0F,false,NULL,false,-1);
  APIExit();
  if(header&&geom) {
    result = Py_BuildValue("(ss)",header,geom);
  }
  VLAFreeP(header);
  VLAFreeP(geom);
  return(APIAutoNone(result));
}

static PyObject *CmdGetMtlObj(PyObject *dummy, PyObject *args)
{
  PyObject *result = NULL;
  char *obj=NULL,*mtl=NULL;
  APIEntry();
  SceneRay(TempPyMOLGlobals,0,0,5,&obj,
           &mtl,0.0F,0.0F,false,NULL,false,-1);
  APIExit();
  if(obj&&mtl) {
    result = Py_BuildValue("(ss)",mtl,obj);
  }
  VLAFreeP(obj);
  VLAFreeP(mtl);
  return(APIAutoNone(result));
}

static PyObject *CmdGetWizard(PyObject *dummy, PyObject *args)
{
  PyObject *result;
  APIEntry();
  result = WizardGet(TempPyMOLGlobals);
  APIExit();
  if(!result)
    result=Py_None;
  return APIIncRef(result);
}

static PyObject *CmdGetWizardStack(PyObject *dummy, PyObject *args)
{
  PyObject *result;
  APIEnterBlocked();
  result = WizardGetStack(TempPyMOLGlobals);
  APIExitBlocked();
  if(!result)
    result=Py_None;
  return APIIncRef(result);
}

static PyObject *CmdSetWizard(PyObject *dummy, PyObject *args)
{
  
  PyObject *obj;
  int ok=false;
  int replace;
  ok = PyArg_ParseTuple(args,"Oi",&obj,&replace);
  if(ok) {
    if(!obj)
      ok=false;
    else
      {
        APIEntry();
        WizardSet(TempPyMOLGlobals,obj,replace); /* TODO STATUS */
        APIExit();
      }
  }
  return APIResultOk(ok);  
}

static PyObject *CmdSetWizardStack(PyObject *dummy, PyObject *args)
{
  
  PyObject *obj;
  int ok=false;
  ok = PyArg_ParseTuple(args,"O",&obj);
  if(ok) {
    if(!obj)
      ok=false;
    else
      {
        APIEntry();
        WizardSetStack(TempPyMOLGlobals,obj); /* TODO STATUS */
        APIExit();
      }
  }
  return APIResultOk(ok);  
}

static PyObject *CmdRefreshWizard(PyObject *dummy, PyObject *args)
{
  APIEntry();
  WizardRefresh(TempPyMOLGlobals);
  OrthoDirty(TempPyMOLGlobals);
  APIExit();
  return APISuccess();  
}

static PyObject *CmdDirtyWizard(PyObject *dummy, PyObject *args)
{
  
  APIEntry();
  WizardDirty(TempPyMOLGlobals);
  APIExit();
  return APISuccess();  
}

static PyObject *CmdSplash(PyObject *dummy, PyObject *args)
{
  int query;
  int result=1;
  
  result = PyArg_ParseTuple(args,"i",&query);
  if(!query) {
    APIEntry();
    OrthoSplash(TempPyMOLGlobals);
    APIExit();
  } else {
/* BEGIN PROPRIETARY CODE SEGMENT (see disclaimer in "os_proprietary.h") */ 
#ifdef _IPYMOL
    result=0;
#else
#ifdef _EPYMOL
    result=2;
#endif
#endif
/* END PROPRIETARY CODE SEGMENT */
  }
  return APIResultCode(result);
}

static PyObject *CmdCls(PyObject *dummy, PyObject *args)
{
  APIEntry();
  OrthoClear(TempPyMOLGlobals);
  APIExit();
  return APISuccess();  
}

static PyObject *CmdDump(PyObject *dummy, PyObject *args)
{
  char *str1,*str2;
  int ok=false;
  ok = PyArg_ParseTuple(args,"ss",&str1,&str2);
  if (ok) {
    APIEntry();
    ExecutiveDump(TempPyMOLGlobals,str1,str2); /* TODO STATUS */
    APIExit();
  }
  return APIResultOk(ok);  
}

static PyObject *CmdIsomesh(PyObject *self, 	PyObject *args) {
  char *str1,*str2,*str3;
  float lvl,fbuf;
  int dotFlag;
  int c,state=-1;
  OrthoLineType s1;
  int oper,frame;
  float carve;
  CObject *obj=NULL,*mObj,*origObj;
  ObjectMap *mapObj;
  float mn[3] = { 0,0,0};
  float mx[3] = { 15,15,15};
  float *vert_vla = NULL;
  int ok = false;
  int map_state;
  int multi=false;
  ObjectMapState *ms;
  int quiet;
  /* oper 0 = all, 1 = sele + buffer, 2 = vector */

  ok = PyArg_ParseTuple(args,"sisisffiifii",&str1,&frame,&str2,&oper,
			&str3,&fbuf,&lvl,&dotFlag,&state,&carve,&map_state,&quiet);
  if (ok) {
    APIEntry();

    origObj=ExecutiveFindObjectByName(TempPyMOLGlobals,str1);  
    if(origObj) {
      if(origObj->type!=cObjectMesh) {
        ExecutiveDelete(TempPyMOLGlobals,str1);
        origObj=NULL;
      }
    }
    
    mObj=ExecutiveFindObjectByName(TempPyMOLGlobals,str2);  
    if(mObj) {
      if(mObj->type!=cObjectMap)
        mObj=NULL;
    }
    if(mObj) {
      mapObj = (ObjectMap*)mObj;
      if(state==-1) {
        multi=true;
        state=0;
        map_state=0;
      } else if(state==-2) {
        state=SceneGetState(TempPyMOLGlobals);
        if(map_state<0) 
          map_state=state;
      } else if(state==-3) { /* append mode */
        state=0;
        if(origObj)
          if(origObj->fGetNFrame)
            state=origObj->fGetNFrame(origObj);
      } else {
        if(map_state==-1) {
          map_state=0;
          multi=true;
        } else {
          multi=false;
        }
      }
      while(1) {
        if(map_state==-2)
          map_state=SceneGetState(TempPyMOLGlobals);
        if(map_state==-3)
          map_state=ObjectMapGetNStates(mapObj)-1;
        ms = ObjectMapStateGetActive(mapObj,map_state);
        if(ms) {
          switch(oper) {
          case 0:
            for(c=0;c<3;c++) {
              mn[c] = ms->Corner[c];
              mx[c] = ms->Corner[3*7+c];
            }
            if(ms->State.Matrix) {
              transform44d3f(ms->State.Matrix,mn,mn);
              transform44d3f(ms->State.Matrix,mx,mx);
              {
                float tmp;
                int a;
                for(a=0;a<3;a++)
                  if(mn[a]>mx[a]) { tmp=mn[a];mn[a]=mx[a];mx[a]=tmp; }
              }
            }
            carve = -0.0; /* impossible */
            break;
          case 1:
            ok = (SelectorGetTmp(TempPyMOLGlobals,str3,s1)>=0);
            ExecutiveGetExtent(TempPyMOLGlobals,s1,mn,mx,false,-1,false); /* TODO state */
            if(carve!=0.0) {
              vert_vla = ExecutiveGetVertexVLA(TempPyMOLGlobals,s1,state);
              if(fbuf<=R_SMALL4)
                fbuf = fabs(carve);
            }
            SelectorFreeTmp(TempPyMOLGlobals,s1);
            for(c=0;c<3;c++) {
              mn[c]-=fbuf;
              mx[c]+=fbuf;
            }
            break;
          }
          PRINTFB(TempPyMOLGlobals,FB_CCmd,FB_Blather)
            " Isomesh: buffer %8.3f carve %8.3f \n",fbuf,carve
            ENDFB(TempPyMOLGlobals);
          obj=(CObject*)ObjectMeshFromBox(TempPyMOLGlobals,(ObjectMesh*)origObj,mapObj,
                                          map_state,state,mn,mx,lvl,dotFlag,
                                          carve,vert_vla);
          if(!origObj) {
            ObjectSetName(obj,str1);
            ExecutiveManageObject(TempPyMOLGlobals,(CObject*)obj,-1,false);
          }
          
          if(SettingGet(TempPyMOLGlobals,cSetting_isomesh_auto_state))
            if(obj) ObjectGotoState((ObjectMolecule*)obj,state);
	  if(!quiet) {
	    PRINTFB(TempPyMOLGlobals,FB_ObjectMesh,FB_Actions)
	      " Isomesh: created \"%s\", setting level to %5.3f\n",str1,lvl
	      ENDFB(TempPyMOLGlobals);
	  }
        } else if(!multi) {
          PRINTFB(TempPyMOLGlobals,FB_ObjectMesh,FB_Warnings)
            " Isomesh-Warning: state %d not present in map \"%s\".\n",map_state+1,str2
            ENDFB(TempPyMOLGlobals);
          ok=false;
        }
        if(multi) {
          origObj = obj;
          map_state++;
          state++;
          if(map_state>=mapObj->NState)
            break;
        } else {
          break;
        }
      }
    } else {
      PRINTFB(TempPyMOLGlobals,FB_ObjectMesh,FB_Errors)
        " Isomesh: Map or brick object \"%s\" not found.\n",str2
        ENDFB(TempPyMOLGlobals);
      ok=false;
    }
    APIExit();
  }
  return APIResultOk(ok);  
}

static PyObject *CmdSliceNew(PyObject *self, 	PyObject *args) 
{
  int ok = true;
  int multi = false;
  char * slice;
  char * map;
  float opacity = -1;
  int state,map_state;
  CObject *obj=NULL,*mObj,*origObj;
  ObjectMap *mapObj;
  ObjectMapState *ms;

  ok = PyArg_ParseTuple(args,"ssii",&slice,&map,&state,&map_state);  
  if (ok) {
    APIEntry();
    if(opacity == -1){
      opacity = 1;
    }
    origObj=ExecutiveFindObjectByName(TempPyMOLGlobals,slice);  
    if(origObj) {
      if(origObj->type!=cObjectSlice) {
        ExecutiveDelete(TempPyMOLGlobals,slice);
        origObj=NULL;
      }
    }

    mObj=ExecutiveFindObjectByName(TempPyMOLGlobals,map);  
    if(mObj) {
      if(mObj->type!=cObjectMap)
        mObj=NULL;
    }
    if(mObj) {
      mapObj = (ObjectMap*)mObj;
      if(state==-1) {
        multi=true;
        state=0;
        map_state=0;
      } else if(state==-2) {
        state=SceneGetState(TempPyMOLGlobals);
        if(map_state<0) 
          map_state=state;
      } else if(state==-3) { /* append mode */
        state=0;
        if(origObj)
          if(origObj->fGetNFrame)
            state=origObj->fGetNFrame(origObj);
      } else {
        if(map_state==-1) {
          map_state=0;
          multi=true;
        } else {
          multi=false;
        }
      }
      while(1) {      
        if(map_state==-2)
          map_state=SceneGetState(TempPyMOLGlobals);
        if(map_state==-3)
          map_state=ObjectMapGetNStates(mapObj)-1;
        ms = ObjectMapStateGetActive(mapObj,map_state);
        if(ms) {
          obj=(CObject*)ObjectSliceFromMap(TempPyMOLGlobals,(ObjectSlice*)origObj,mapObj,
                                           state,map_state);
     
          if(!origObj) {
            ObjectSetName(obj,slice);
            ExecutiveManageObject(TempPyMOLGlobals,(CObject*)obj,-1,false);
          }
          PRINTFB(TempPyMOLGlobals,FB_ObjectMesh,FB_Actions)
            " SliceMap: created \"%s\", setting opacity to %5.3f\n",slice,opacity
            ENDFB(TempPyMOLGlobals);

        } else if(!multi) {
          PRINTFB(TempPyMOLGlobals,FB_ObjectSlice
                  ,FB_Warnings)
            " SliceMap-Warning: state %d not present in map \"%s\".\n",map_state+1,map
            ENDFB(TempPyMOLGlobals);
          ok=false;
        }
        if(multi) {
          origObj = obj;
          map_state++;
          state++;
          if(map_state>=mapObj->NState)
            break;
        } else {
          break;
        }
      }      
    }
  }else{
    PRINTFB(TempPyMOLGlobals,FB_ObjectSlice,FB_Errors)
      " SliceMap: Map or brick object \"%s\" not found.\n",map
      ENDFB(TempPyMOLGlobals);
    ok=false;
  }
  APIExit();
  return APIResultOk(ok);  
}
#if 0

static PyObject *CmdRGBFunction(PyObject *self, 	PyObject *args) {
  int ok = true;
  int multi = false;
  char * slice;
  int function = -1;
  int state;
  CObject *obj=NULL;
  ObjectSlice *Sobj=NULL;
  ObjectSliceState *ss;
  
  ok = PyArg_ParseTuple(args,"sii",&slice,&function,&state);
  if (ok) {
    APIEntry();
    obj=ExecutiveFindObjectByName(TempPyMOLGlobals,slice);  
    if(obj) {
      if(obj->type!=cObjectSlice) {
        obj=NULL;
        ok=false;
      }
    }
    if(obj) {
      Sobj = (ObjectSlice*)obj;
      if(state==-1) {
        multi=true;
        state=0;
      } else if(state==-2) {
        state=SceneGetState(TempPyMOLGlobals);
        multi=false;
      } else {
        multi=false;
      }
      while(1) {      
        ss = ObjectSliceStateGetActive(Sobj,state);
        if(ss) {
          ss->RGBFunction = function;
          ss->RefreshFlag = true;
        }
        if(multi) {
          state++;
          if(state>=Sobj->NState)
            break;
        } else {
          break;
        }
      }      
    }else{
      PRINTFB(TempPyMOLGlobals,FB_ObjectSlice,FB_Errors)
        " SliceRGBFunction-Warning: Object \"%s\" doesn't exist or is not a slice.\n",slice
        ENDFB(TempPyMOLGlobals);
      ok=false;
    }
    APIExit();
  }
  return APIResultOk(ok);  
}


static PyObject *CmdSliceHeightmap(PyObject *self, 	PyObject *args) {
  int ok = true;
  int multi = false;
  char * slice;
  int state;
  CObject *obj=NULL;
  ObjectSlice *Sobj=NULL;
  ObjectSliceState *ss;
  
  ok = PyArg_ParseTuple(args,"si",&slice,&state);  
  if (ok) {
    APIEntry();
    obj=ExecutiveFindObjectByName(TempPyMOLGlobals,slice);  
    if(obj) {
      if(obj->type!=cObjectSlice) {
        obj=NULL;
        ok=false;
      }
    }
    if(obj) {
      Sobj = (ObjectSlice*)obj;
      if(state==-1) {
        multi=true;
        state=0;
      } else if(state==-2) {
        state=SceneGetState(TempPyMOLGlobals);
        multi=false;
      } else {
        multi=false;
      }
      while(1) {      
        ss = ObjectSliceStateGetActive(Sobj,state);
        if(ss) {
          ss->HeightmapFlag = !ss->HeightmapFlag;
        }
        if(multi) {
          state++;
          if(state>=Sobj->NState)
            break;
        } else {
          break;
        }
      }      
    }else{
      PRINTFB(TempPyMOLGlobals,FB_ObjectSlice,FB_Errors)
        " SliceHeightmap-Warning: Object \"%s\" doesn't exist or is not a slice.\n",slice
        ENDFB(TempPyMOLGlobals);
      ok=false;
    }
    APIExit();
  }
  return APIResultOk(ok);  
}


static PyObject *CmdSliceSetLock(PyObject *self, 	PyObject *args) {
  int ok = true;
  int multi = false;
  char * slice;
  int lock = -1;
  int state;
  CObject *obj=NULL;
  ObjectSlice *Sobj=NULL;
  ObjectSliceState *ss;
  
  ok = PyArg_ParseTuple(args,"sii",&slice,&state,&lock);  
  if (ok) {
    APIEntry();
    obj=ExecutiveFindObjectByName(TempPyMOLGlobals,slice);  
    if(obj) {
      if(obj->type!=cObjectSlice) {
        obj=NULL;
        ok=false;
      }
    }
    if(obj) {
      Sobj = (ObjectSlice*)obj;
      if(state==-1) {
        multi=true;
        state=0;
      } else if(state==-2) {
        state=SceneGetState(TempPyMOLGlobals);
        multi=false;
      } else {
        multi=false;
      }
      while(1) {      
        ss = ObjectSliceStateGetActive(Sobj,state);
        if(ss) {
          ss->LockedFlag = lock;
          ss->RefreshFlag = true;
        }
        if(multi) {
          state++;
          if(state>=Sobj->NState)
            break;
        } else {
          break;
        }
      }      
    }else{
      PRINTFB(TempPyMOLGlobals,FB_ObjectSlice,FB_Errors)
        " SliceSetLock-Warning: Object \"%s\" doesn't exist or is not a slice.\n",slice
        ENDFB(TempPyMOLGlobals);
      ok=false;
    }
    APIExit();
  }
  return APIResultOk(ok);  
}
#endif

static PyObject *CmdIsosurface(PyObject *self, 	PyObject *args) {
  char *str1,*str2,*str3;
  float lvl,fbuf;
  int dotFlag;
  int c,state=-1;
  OrthoLineType s1;
  int oper,frame;
  float carve;
  CObject *obj=NULL,*mObj,*origObj;
  ObjectMap *mapObj;
  float mn[3] = { 0,0,0};
  float mx[3] = { 15,15,15};
  float *vert_vla = NULL;
  int ok = false;
  ObjectMapState *ms;
  int map_state=0;
  int multi=false;
  int side;
  int quiet;
  /* oper 0 = all, 1 = sele + buffer, 2 = vector */

  ok = PyArg_ParseTuple(args,"sisisffiifiii",&str1,&frame,&str2,&oper,
                   &str3,&fbuf,&lvl,&dotFlag,&state,&carve,&map_state,
                        &side,&quiet);
  if (ok) {
    APIEntry();

    origObj=ExecutiveFindObjectByName(TempPyMOLGlobals,str1);  
    if(origObj) {
      if(origObj->type!=cObjectSurface) {
        ExecutiveDelete(TempPyMOLGlobals,str1);
        origObj=NULL;
      }
    }
    
    mObj=ExecutiveFindObjectByName(TempPyMOLGlobals,str2);  
    if(mObj) {
      if(mObj->type!=cObjectMap)
        mObj=NULL;
    }
    if(mObj) {
      mapObj = (ObjectMap*)mObj;
      if(state==-1) {
        multi=true;
        state=0;
        map_state=0;
      } else if(state==-2) { /* current state */
        state=SceneGetState(TempPyMOLGlobals);
        if(map_state<0) 
          map_state=state;
      } else if(state==-3) { /* append mode */
        state=0;
        if(origObj)
          if(origObj->fGetNFrame)
            state=origObj->fGetNFrame(origObj);
      } else {
        if(map_state==-1) {
          map_state=0;
          multi=true;
        } else {
          multi=false;
        }
      }
      while(1) {
        if(map_state==-2)
          map_state=SceneGetState(TempPyMOLGlobals);
        if(map_state==-3)
          map_state=ObjectMapGetNStates(mapObj)-1;
        ms = ObjectMapStateGetActive(mapObj,map_state);
        if(ms) {
          switch(oper) {
          case 0:
            for(c=0;c<3;c++) {
              mn[c] = ms->Corner[c];
              mx[c] = ms->Corner[3*7+c];
            }
            if(ms->State.Matrix) {
              transform44d3f(ms->State.Matrix,mn,mn);
              transform44d3f(ms->State.Matrix,mx,mx);
              {
                float tmp;
                int a;
                for(a=0;a<3;a++)
                  if(mn[a]>mx[a]) { tmp=mn[a];mn[a]=mx[a];mx[a]=tmp; }
              }
            }
            carve = 0.0F;
            break;
          case 1:
            ok = (SelectorGetTmp(TempPyMOLGlobals,str3,s1)>=0);
            ExecutiveGetExtent(TempPyMOLGlobals,s1,mn,mx,false,-1,false); /* TODO state */
            if(carve!=0.0F) {
              vert_vla = ExecutiveGetVertexVLA(TempPyMOLGlobals,s1,state);
              if(fbuf<=R_SMALL4)
                fbuf = fabs(carve);
            }
            SelectorFreeTmp(TempPyMOLGlobals,s1);
            for(c=0;c<3;c++) {
              mn[c]-=fbuf;
              mx[c]+=fbuf;
            }
            break;
          }
          PRINTFB(TempPyMOLGlobals,FB_CCmd,FB_Blather)
            " Isosurface: buffer %8.3f carve %8.3f\n",fbuf,carve
            ENDFB(TempPyMOLGlobals);
          obj=(CObject*)ObjectSurfaceFromBox(TempPyMOLGlobals,(ObjectSurface*)origObj,mapObj,map_state,
                                             state,mn,mx,lvl,dotFlag,
                                             carve,vert_vla,side);
          if(!origObj) {
            ObjectSetName(obj,str1);
            ExecutiveManageObject(TempPyMOLGlobals,(CObject*)obj,-1,false);
          }
          if(SettingGet(TempPyMOLGlobals,cSetting_isomesh_auto_state))
            if(obj) ObjectGotoState((ObjectMolecule*)obj,state);
	  if(!quiet) {
	    PRINTFB(TempPyMOLGlobals,FB_ObjectSurface,FB_Actions)
	      " Isosurface: created \"%s\", setting level to %5.3f\n",str1,lvl
	      ENDFB(TempPyMOLGlobals);
	  }
        } else if(!multi) {
          PRINTFB(TempPyMOLGlobals,FB_ObjectMesh,FB_Warnings)
            " Isosurface-Warning: state %d not present in map \"%s\".\n",map_state+1,str2
            ENDFB(TempPyMOLGlobals);
          ok=false;
        }
        if(multi) {
          origObj = obj;
          map_state++;
          state++;
          if(map_state>=mapObj->NState)
            break;
        } else {
          break;
        }
      }
    } else {
      PRINTFB(TempPyMOLGlobals,FB_ObjectSurface,FB_Errors)
        " Isosurface: Map or brick object \"%s\" not found.\n",str2
        ENDFB(TempPyMOLGlobals);
      ok=false;
    }
    APIExit();
  }
  return APIResultOk(ok);  
}

static PyObject *CmdSymExp(PyObject *self, 	PyObject *args) {
  char *str1,*str2,*str3;
  OrthoLineType s1;
  float cutoff;
  CObject *mObj;
  int segi;
  int quiet;
  /* oper 0 = all, 1 = sele + buffer, 2 = vector */

  int ok=false;
  ok = PyArg_ParseTuple(args,"sssfii",&str1,&str2,&str3,&cutoff,&segi,&quiet);
  if (ok) {
    APIEntry();
    mObj=ExecutiveFindObjectByName(TempPyMOLGlobals,str2);  
    if(mObj) {
      if(mObj->type!=cObjectMolecule) {
        mObj=NULL;
        ok = false;
      }
    }
    if(mObj) {
      ok = (SelectorGetTmp(TempPyMOLGlobals,str3,s1)>=0);
      if(ok) 
        ExecutiveSymExp(TempPyMOLGlobals,str1,str2,s1,cutoff,segi,quiet); /* TODO STATUS */
      SelectorFreeTmp(TempPyMOLGlobals,s1);
    }
    APIExit();
  }
  return APIResultOk(ok);  
}

static PyObject *CmdOverlap(PyObject *dummy, PyObject *args)
{
  char *str1,*str2;
  int state1,state2;
  float overlap = -1.0;
  float adjust;
  OrthoLineType s1,s2;
  int ok=false;
  ok = PyArg_ParseTuple(args,"ssiif",&str1,&str2,&state1,&state2,&adjust);
  if (ok) {
    APIEntry();
    ok = ((SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0) &&
          (SelectorGetTmp(TempPyMOLGlobals,str2,s2)>=0));
    if(ok) {
      overlap = ExecutiveOverlap(TempPyMOLGlobals,s1,state1,s2,state2,adjust);
    }
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    SelectorFreeTmp(TempPyMOLGlobals,s2);
    APIExit();
  }
  return(Py_BuildValue("f",overlap));
}

static PyObject *CmdDist(PyObject *dummy, PyObject *args)
{
  char *name,*str1,*str2;
  float cutoff,result=-1.0;
  int labels,quiet;
  int mode,reset,state,zoom;
  OrthoLineType s1,s2;
  int ok=false;
  int c1,c2;
  ok = PyArg_ParseTuple(args,"sssifiiiii",&name,&str1,
                        &str2,&mode,&cutoff,
                        &labels,&quiet,&reset,&state,&zoom);
  if (ok) {
    APIEntry();
    c1 = SelectorGetTmp(TempPyMOLGlobals,str1,s1);
    c2 = SelectorGetTmp(TempPyMOLGlobals,str2,s2);
    if((c1<0)||(c2<0))
      ok = false;
    else {
      if(c1&&(c2||WordMatch(TempPyMOLGlobals,cKeywordSame,s2,true)))
        ExecutiveDist(TempPyMOLGlobals,&result,name,s1,s2,mode,cutoff,
                      labels,quiet,reset,state,zoom);
      else {
        if((!quiet)&&(!c1)) {
          PRINTFB(TempPyMOLGlobals,FB_Executive,FB_Errors)
            "Distance-Error: selection 1 contains no atoms.\n"
            ENDFB(TempPyMOLGlobals);
          if(reset)
            ExecutiveDelete(TempPyMOLGlobals,name);
        } 
        if((!quiet)&&(!c2)) {
          PRINTFB(TempPyMOLGlobals,FB_Executive,FB_Errors)
            "Distance-Error: selection 2 contains no atoms.\n"
            ENDFB(TempPyMOLGlobals);
          if(reset)
            ExecutiveDelete(TempPyMOLGlobals,name);
        }
        result = -1.0;
      }
    }
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    SelectorFreeTmp(TempPyMOLGlobals,s2);
    APIExit();
  }
  if(!ok) 
    return APIFailure();
  else 
    return(Py_BuildValue("f",result));
}

static PyObject *CmdAngle(PyObject *dummy, PyObject *args)
{
  char *name,*str1,*str2,*str3;
  float result=-999.0;
  int labels,quiet;
  int mode;
  OrthoLineType s1,s2,s3;
  int ok=false;
  int c1,c2,c3;
  int reset, zoom;
  int state;
  ok = PyArg_ParseTuple(args,"ssssiiiiii",
                        &name,&str1,&str2,&str3,
                        &mode,&labels,&reset,&zoom,&quiet,&state);
  if (ok) {
    APIEntry();
    c1 = SelectorGetTmp(TempPyMOLGlobals,str1,s1);
    c2 = SelectorGetTmp(TempPyMOLGlobals,str2,s2);
    c3 = SelectorGetTmp(TempPyMOLGlobals,str3,s3);
    if(c1&&
       (c2||WordMatch(TempPyMOLGlobals,cKeywordSame,s2,true))&&
       (c3||WordMatch(TempPyMOLGlobals,cKeywordSame,s3,true)))
      ExecutiveAngle(TempPyMOLGlobals,&result,name,s1,s2,s3,
                              mode,labels,reset,zoom,quiet,state);
    else {
      if((!quiet)&&(!c1)) {
        PRINTFB(TempPyMOLGlobals,FB_Executive,FB_Errors)
          " Distance-ERR: selection 1 contains no atoms.\n"
          ENDFB(TempPyMOLGlobals);
      } 
      if((quiet!=2)&&(!c2)) {
        PRINTFB(TempPyMOLGlobals,FB_Executive,FB_Errors)
          " Distance-ERR: selection 2 contains no atoms.\n"
          ENDFB(TempPyMOLGlobals);
      }
      if((quiet!=2)&&(!c3)) {
        PRINTFB(TempPyMOLGlobals,FB_Executive,FB_Errors)
          " Distance-ERR: selection 3 contains no atoms.\n"
          ENDFB(TempPyMOLGlobals);
      }
      result = -1.0;
    }
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    SelectorFreeTmp(TempPyMOLGlobals,s2);
    SelectorFreeTmp(TempPyMOLGlobals,s3);
    APIExit();
  }
  return(Py_BuildValue("f",result));
}

static PyObject *CmdDihedral(PyObject *dummy, PyObject *args)
{
  char *name,*str1,*str2,*str3,*str4;
  float result=-999.0;
  int labels,quiet;
  int mode;
  OrthoLineType s1,s2,s3,s4;
  int ok=false;
  int c1,c2,c3,c4;
  int reset, zoom;
  int state;
  ok = PyArg_ParseTuple(args,"sssssiiiiii",
                        &name,&str1,&str2,&str3,&str4,
                        &mode,&labels,&reset,&zoom,&quiet,&state);
  if (ok) {
    APIEntry();
    c1 = SelectorGetTmp(TempPyMOLGlobals,str1,s1);
    c2 = SelectorGetTmp(TempPyMOLGlobals,str2,s2);
    c3 = SelectorGetTmp(TempPyMOLGlobals,str3,s3);
    c4 = SelectorGetTmp(TempPyMOLGlobals,str4,s4);
    if(c1&&
       (c2||WordMatch(TempPyMOLGlobals,cKeywordSame,s2,true))&&
       (c3||WordMatch(TempPyMOLGlobals,cKeywordSame,s3,true))&&
       (c4||WordMatch(TempPyMOLGlobals,cKeywordSame,s4,true)))
      ExecutiveDihedral(TempPyMOLGlobals,&result,name,s1,s2,s3,s4,
                                 mode,labels,reset,zoom,quiet,state);
    else {
      if((!quiet)&&(!c1)) {
        PRINTFB(TempPyMOLGlobals,FB_Executive,FB_Errors)
          " Distance-ERR: selection 1 contains no atoms.\n"
          ENDFB(TempPyMOLGlobals);
      } 
      if((quiet!=2)&&(!c2)) {
        PRINTFB(TempPyMOLGlobals,FB_Executive,FB_Errors)
          " Distance-ERR: selection 2 contains no atoms.\n"
          ENDFB(TempPyMOLGlobals);
      }
      if((quiet!=2)&&(!c3)) {
        PRINTFB(TempPyMOLGlobals,FB_Executive,FB_Errors)
          " Distance-ERR: selection 3 contains no atoms.\n"
          ENDFB(TempPyMOLGlobals);
      }
      if((quiet!=2)&&(!c4)) {
        PRINTFB(TempPyMOLGlobals,FB_Executive,FB_Errors)
          " Distance-ERR: selection 4 contains no atoms.\n"
          ENDFB(TempPyMOLGlobals);
      }
      result = -1.0;
    }
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    SelectorFreeTmp(TempPyMOLGlobals,s2);
    SelectorFreeTmp(TempPyMOLGlobals,s3);
    SelectorFreeTmp(TempPyMOLGlobals,s4);
    APIExit();
  }
  return(Py_BuildValue("f",result));
}


static PyObject *CmdBond(PyObject *dummy, PyObject *args)
{
  char *str1,*str2;
  int order,mode;
  OrthoLineType s1,s2;
  int ok=false;
  ok = PyArg_ParseTuple(args,"ssii",&str1,&str2,&order,&mode);
  if (ok) {
    APIEntry();
    ok = ((SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0) &&
          (SelectorGetTmp(TempPyMOLGlobals,str2,s2)>=0));
    if(ok) 
      ExecutiveBond(TempPyMOLGlobals,s1,s2,order,mode); /* TODO STATUS */
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    SelectorFreeTmp(TempPyMOLGlobals,s2);
    APIExit();
  }
  return APIResultOk(ok);  
}

static PyObject *CmdVdwFit(PyObject *dummy, PyObject *args)
{
  char *str1,*str2;
  int state1,state2,quiet;
  float buffer;
  OrthoLineType s1,s2;
  int ok=false;
  ok = PyArg_ParseTuple(args,"sisifi",&str1,&state1,&str2,&state2,&buffer,&quiet);
  if (ok) {
    APIEntry();
    ok = ((SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0) &&
          (SelectorGetTmp(TempPyMOLGlobals,str2,s2)>=0));
    if(ok) 
      ok = ExecutiveVdwFit(TempPyMOLGlobals,s1,state1,s2,state2,buffer,quiet); 
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    SelectorFreeTmp(TempPyMOLGlobals,s2);
    APIExit();
  }
  return APIResultOk(ok);  
}

static PyObject *CmdLabel(PyObject *self,   PyObject *args)
{
  char *str1,*str2;
  OrthoLineType s1;
  int quiet;
  int ok=false;
  ok = PyArg_ParseTuple(args,"ssi",&str1,&str2,&quiet);
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    if(ok) ok=ExecutiveLabel(TempPyMOLGlobals,s1,str2,quiet,true); /* TODO STATUS */
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);

}

static PyObject *CmdAlter(PyObject *self,   PyObject *args)
{
  char *str1,*str2;
  int i1,quiet;
  OrthoLineType s1;
  int result=0;
  int ok=false;
  PyObject *space;
  ok = PyArg_ParseTuple(args,"ssiiO",&str1,&str2,&i1,&quiet,&space);
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    result=ExecutiveIterate(TempPyMOLGlobals,s1,str2,i1,quiet,space); /* TODO STATUS */
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return Py_BuildValue("i",result);
}

static PyObject *CmdAlterList(PyObject *self,   PyObject *args)
{
  char *str1;
  OrthoLineType s1;
  int quiet;
  int result=0;
  int ok=false;
  PyObject *space;
  PyObject *list;
  ok = PyArg_ParseTuple(args,"sOiO",&str1,&list,&quiet,&space);
  if (ok) {
    APIEnterBlocked();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    result=ExecutiveIterateList(TempPyMOLGlobals,s1,list,false,quiet,space); /* TODO STATUS */
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExitBlocked();
  }
  return Py_BuildValue("i",result);
}

static PyObject *CmdSelectList(PyObject *self,   PyObject *args)
{
  char *str1,*sele_name;
  OrthoLineType s1;
  int quiet;
  int result=0;
  int ok=false;
  int mode;
  int state;
  PyObject *list;
  ok = PyArg_ParseTuple(args,"ssOiii",&sele_name,&str1,&list,&state,&mode,&quiet);
  if (ok) { 
    int *int_array = NULL;
    APIEnterBlocked(); 
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    if(ok) ok = PyList_Check(list);
    if(ok) ok = PConvPyListToIntArray(list,&int_array);
    if(ok) {
      int list_len = PyList_Size(list);
      result=ExecutiveSelectList(TempPyMOLGlobals,sele_name,s1,int_array,list_len,state,mode,quiet); 
      SceneInvalidate(TempPyMOLGlobals);
      SeqDirty(TempPyMOLGlobals);
    }
    FreeP(int_array);
    APIExitBlocked();
  }
  return Py_BuildValue("i",result);
}


static PyObject *CmdAlterState(PyObject *self,   PyObject *args)
{
  char *str1,*str2;
  int i1,i2,i3,quiet;
  OrthoLineType s1;
  PyObject *obj;
  int ok=false;
  ok = PyArg_ParseTuple(args,"issiiiO",&i1,&str1,&str2,&i2,&i3,&quiet,&obj);
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    ExecutiveIterateState(TempPyMOLGlobals,i1,s1,str2,i2,i3,quiet,obj); /* TODO STATUS */
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);

}

static PyObject *CmdCopy(PyObject *self,   PyObject *args)
{
  char *str1,*str2;
  int ok=false;
  int zoom;
  ok = PyArg_ParseTuple(args,"ssi",&str1,&str2,&zoom);
  if (ok) {
    APIEntry();
    ExecutiveCopy(TempPyMOLGlobals,str1,str2,zoom); /* TODO STATUS */
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdRecolor(PyObject *self,   PyObject *args)
{
  char *str1;
  OrthoLineType s1;
  int ok=true;
  int rep=-1;
  ok = PyArg_ParseTuple(args,"si",&str1,&rep);
  PRINTFD(TempPyMOLGlobals,FB_CCmd)
    " CmdRecolor: called with %s.\n",str1
    ENDFD;

  if (ok) {
    APIEntry();
    if(WordMatch(TempPyMOLGlobals,str1,"all",true)<0)
      ExecutiveInvalidateRep(TempPyMOLGlobals,str1,rep,cRepInvColor);
    else {
      ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
      ExecutiveInvalidateRep(TempPyMOLGlobals,s1,rep,cRepInvColor);
      SelectorFreeTmp(TempPyMOLGlobals,s1); 
    }
    APIExit();
  } else {
    ok = -1; /* special error convention */
  }
  return APIResultOk(ok);
}

static PyObject *CmdRebuild(PyObject *self,   PyObject *args)
{
  char *str1;
  OrthoLineType s1;
  int ok=true;
  int rep=-1;
  ok = PyArg_ParseTuple(args,"si",&str1,&rep);
  PRINTFD(TempPyMOLGlobals,FB_CCmd)
    " CmdRebuild: called with %s.\n",str1
    ENDFD;

  if (ok) {
    APIEntry();
    if(WordMatch(TempPyMOLGlobals,str1,"all",true)<0)
      ExecutiveRebuildAll(TempPyMOLGlobals);
    else {
      ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
      if(SettingGetGlobal_b(TempPyMOLGlobals,cSetting_defer_builds_mode))
        ExecutiveInvalidateRep(TempPyMOLGlobals,s1,rep,cRepInvPurge);        
      else
        ExecutiveInvalidateRep(TempPyMOLGlobals,s1,rep,cRepInvAll);
      SelectorFreeTmp(TempPyMOLGlobals,s1); 
    }
    APIExit();
  } else {
    ok = -1; /* special error convention */
  }
  return APIResultOk(ok);
}

static PyObject *CmdResetRate(PyObject *dummy, PyObject *args)
{
  APIEntry();
  ButModeResetRate(TempPyMOLGlobals);
  APIExit();
  return APISuccess();
}

static PyObject *CmdReady(PyObject *dummy, PyObject *args)
{
  if(TempPyMOLGlobals)
    return(APIResultCode(TempPyMOLGlobals->Ready));
  else
    return(APIResultCode(0));
}

#if 0
extern int _Py_CountReferences(void);
#endif
static PyObject *CmdMem(PyObject *dummy, PyObject *args)
{
  MemoryDebugDump();
  OVHeap_Dump(TempPyMOLGlobals->Context->heap,0);
  SelectorMemoryDump(TempPyMOLGlobals);
  ExecutiveMemoryDump(TempPyMOLGlobals);
#if 0
  printf(" Py_Debug: %d total references.\n",_Py_CountReferences());
#endif
  return APISuccess();
}

static int decoy_input_hook(void)
{
  return 0;
}

static PyObject *CmdRunPyMOL(PyObject *dummy, PyObject *args)
{
#ifdef _PYMOL_NO_MAIN
  exit(0);
#else
#ifndef _PYMOL_WX_GLUT

  if(run_only_once) {
    run_only_once=false;

#ifdef _PYMOL_MODULE
    {
      int block_input_hook = false;
      if(!PyArg_ParseTuple(args,"i",&block_input_hook))
        block_input_hook = false;

      /* prevent Tcl/Tk from installing/using its hook, which will
         cause a segmentation fault IF and ONLY IF (1) Tcl/Tk is
         running in a sub-thread (always the case with PyMOL_) and (2)
         when the Python interpreter itself is reading from stdin
         (only the case when launched via "import pymol" with
         launch_mode 2 (async threaded) */

      if(block_input_hook)
        PyOS_InputHook = decoy_input_hook; 
            
      was_main();
    }
#endif
  }
#endif
#endif

  return APISuccess();
}

static PyObject *CmdRunWXPyMOL(PyObject *dummy, PyObject *args)
{
#ifdef _PYMOL_WX_GLUT
#ifndef _PYMOL_ACTIVEX
#ifndef _PYMOL_EMBEDDED
  if(run_only_once) {
    run_only_once=false;
    was_main();
  }
#endif
#endif
#endif

  return APISuccess();
}

static PyObject *CmdCountStates(PyObject *dummy, PyObject *args)
{
  char *str1;
  OrthoLineType s1;
  int ok=false;
  int count = 0;
  ok = PyArg_ParseTuple(args,"s",&str1);
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    count = ExecutiveCountStates(TempPyMOLGlobals,s1);
    if(count<0)
      ok=false;
    SelectorFreeTmp(TempPyMOLGlobals,s1); 
    APIExit();
  }
  return ok ? APIResultCode(count) : APIFailure();
}

static PyObject *CmdCountFrames(PyObject *dummy, PyObject *args)
{
  int result;
  APIEntry();
  SceneCountFrames(TempPyMOLGlobals);
  result=SceneGetNFrame(TempPyMOLGlobals);
  APIExit();
  return(APIResultCode(result));
}

static PyObject *CmdIdentify(PyObject *dummy, PyObject *args)
{
  char *str1;
  OrthoLineType s1;
  int mode;
  int a,l=0;
  PyObject *result = Py_None;
  PyObject *tuple;
  int *iVLA=NULL,*i;
  ObjectMolecule **oVLA=NULL,**o;
  int ok=false;
  ok = PyArg_ParseTuple(args,"si",&str1,&mode);
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    if(ok) {
      if(!mode) {
        iVLA=ExecutiveIdentify(TempPyMOLGlobals,s1,mode);
      } else {
        l = ExecutiveIdentifyObjects(TempPyMOLGlobals,s1,mode,&iVLA,&oVLA);
      }
    }
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
    if(iVLA) {
      if(!mode) {
        result=PConvIntVLAToPyList(iVLA);
      } else { /* object mode */
        result=PyList_New(l);
        i = iVLA;
        o = oVLA;
        for(a=0;a<l;a++) {
          tuple = PyTuple_New(2);      
          PyTuple_SetItem(tuple,1,PyInt_FromLong(*(i++))); 
          PyTuple_SetItem(tuple,0,PyString_FromString((*(o++))->Obj.Name));
          PyList_SetItem(result,a,tuple);
        }
      }
    } else {
      result = PyList_New(0);
    }
  }
  VLAFreeP(iVLA);
  VLAFreeP(oVLA);
  if(!ok) {
    if(result && (result!=Py_None)) {
      Py_DECREF(result);
    }
    return APIFailure();
  } else {
    return(APIAutoNone(result));
  }
}

static PyObject *CmdIndex(PyObject *dummy, PyObject *args)
{
  char *str1;
  OrthoLineType s1;
  int mode;
  PyObject *result = Py_None;
  PyObject *tuple = Py_None;
  int *iVLA=NULL;
  int l = 0;
  int *i;
  ObjectMolecule **o,**oVLA=NULL;
  int a;

  int ok=false;
  ok = PyArg_ParseTuple(args,"si",&str1,&mode);
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    if(ok) l = ExecutiveIndex(TempPyMOLGlobals,s1,mode,&iVLA,&oVLA);
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
    if(iVLA) {
      result=PyList_New(l);
      i = iVLA;
      o = oVLA;
      for(a=0;a<l;a++) {
        tuple = PyTuple_New(2);      
        PyTuple_SetItem(tuple,1,PyInt_FromLong(*(i++)+1)); /* +1 for index */
        PyTuple_SetItem(tuple,0,PyString_FromString((*(o++))->Obj.Name));
        PyList_SetItem(result,a,tuple);
      }
    } else {
      result = PyList_New(0);
    }
    VLAFreeP(iVLA);
    VLAFreeP(oVLA);
  }
  if(!ok) {
    if(result && (result!=Py_None)) {
      Py_DECREF(result);
    }
    return APIFailure();
  } else {
    return(APIAutoNone(result));
  }
}


static PyObject *CmdFindPairs(PyObject *dummy, PyObject *args)
{
  char *str1,*str2;
  int state1,state2;
  float cutoff;
  float angle;
  int mode;
  OrthoLineType s1,s2;
  PyObject *result = Py_None;
  PyObject *tuple = Py_None;
  PyObject *tuple1 = Py_None;
  PyObject *tuple2 = Py_None;
  int *iVLA=NULL;
  int l;
  int *i;
  ObjectMolecule **o,**oVLA=NULL;
  int a;
  
  int ok=false;
  ok = PyArg_ParseTuple(args,"ssiiiff",&str1,&str2,&state1,&state2,&mode,&cutoff,&angle);
  if (ok) {
    APIEntry();
    ok = ((SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0)&&
          (SelectorGetTmp(TempPyMOLGlobals,str2,s2)>=0));
    l = ExecutivePairIndices(TempPyMOLGlobals,s1,s2,state1,state2,mode,cutoff,angle,&iVLA,&oVLA);
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    SelectorFreeTmp(TempPyMOLGlobals,s2);
    APIExit();
    
    if(iVLA&&oVLA) {
      result=PyList_New(l);
      i = iVLA;
      o = oVLA;
      for(a=0;a<l;a++) {
        tuple1 = PyTuple_New(2);      
        PyTuple_SetItem(tuple1,0,PyString_FromString((*(o++))->Obj.Name));
        PyTuple_SetItem(tuple1,1,PyInt_FromLong(*(i++)+1)); /* +1 for index */
        tuple2 = PyTuple_New(2);
        PyTuple_SetItem(tuple2,0,PyString_FromString((*(o++))->Obj.Name));
        PyTuple_SetItem(tuple2,1,PyInt_FromLong(*(i++)+1)); /* +1 for index */
        tuple = PyTuple_New(2);
        PyTuple_SetItem(tuple,0,tuple1);
        PyTuple_SetItem(tuple,1,tuple2);
        PyList_SetItem(result,a,tuple);
      }
    } else {
      result = PyList_New(0);
    }
    VLAFreeP(iVLA);
    VLAFreeP(oVLA);
  }
  return(APIAutoNone(result));
}

static PyObject *CmdSystem(PyObject *dummy, PyObject *args)
{
  char *str1;
  int ok=false;
  int async;
  ok = PyArg_ParseTuple(args,"si",&str1,&async);
  if (ok) {
    if(async) {
      PUnblock(); /* free up PyMOL and the API */
    } else {
      APIEntry(); /* keep PyMOL locked */
    }
    ok = system(str1);
    if(async) {
      PBlock();
    } else {
      APIExit();
    }
  }
  return APIResultOk(ok);
}

static PyObject *CmdGetFeedback(PyObject *dummy, PyObject *args)
{
  if(TempPyMOLGlobals && TempPyMOLGlobals->Ready) {
    PyObject *result = NULL;
    OrthoLineType buffer;
    int ok;
    
    if(TempPyMOLGlobals->Terminating) { /* try to bail */
/* BEGIN PROPRIETARY CODE SEGMENT (see disclaimer in "os_proprietary.h") */ 
#ifdef WIN32
      abort();
#endif
/* END PROPRIETARY CODE SEGMENT */
      exit(0);
    }
    APIEnterBlocked();
    ok = OrthoFeedbackOut(TempPyMOLGlobals,buffer); 
    APIExitBlocked();
    if(ok) result = Py_BuildValue("s",buffer);
    return(APIAutoNone(result));
  } else {
    return(APIAutoNone(NULL));
  }
}

static PyObject *CmdGetSeqAlignStr(PyObject *dummy, PyObject *args)
{
  char *str1;
  char *seq = NULL;
  int state;
  int format;
  int quiet;
  PyObject *result = NULL;
  int ok=false;
  ok = PyArg_ParseTuple(args,"siii",&str1,&state,&format,&quiet);
  if (ok) {
    APIEntry();
    seq=ExecutiveNameToSeqAlignStrVLA(TempPyMOLGlobals,str1,state,format,quiet);
    APIExit();
    if(seq) result = Py_BuildValue("s",seq);
    VLAFreeP(seq);
  }
  return(APIAutoNone(result));
}

static PyObject *CmdGetPDB(PyObject *dummy, PyObject *args)
{
  char *str1;
  char *pdb = NULL;
  int state;
  int quiet;
  char *ref_object = NULL;
  int ref_state;
  int mode;
  OrthoLineType s1 = "";
  PyObject *result = NULL;
  int ok=false;
  ok = PyArg_ParseTuple(args,"siisii",&str1,&state,&mode,&ref_object,&ref_state,&quiet);
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    pdb=ExecutiveSeleToPDBStr(TempPyMOLGlobals,s1,state,true,mode,ref_object,ref_state,quiet);
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
    if(pdb) result = Py_BuildValue("s",pdb);
    FreeP(pdb);
  }
  return(APIAutoNone(result));
}

static PyObject *CmdGetModel(PyObject *dummy, PyObject *args)
{
  char *str1;
  int state;
  OrthoLineType s1;
  PyObject *result = NULL;
  int ok=false;
  ok = PyArg_ParseTuple(args,"si",&str1,&state);
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    if(ok) result=ExecutiveSeleToChemPyModel(TempPyMOLGlobals,s1,state);
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return(APIAutoNone(result));
}

static PyObject *CmdCreate(PyObject *dummy, PyObject *args)
{
  char *str1,*str2;
  int target,source,discrete,quiet;
  int singletons;
  OrthoLineType s1;
  int ok=false;
  int zoom;
  ok = PyArg_ParseTuple(args,"ssiiiiii",&str1,&str2,&source,
                        &target,&discrete,&zoom,&quiet,&singletons);
  if (ok) {
    APIEntry();
    ok=(SelectorGetTmp(TempPyMOLGlobals,str2,s1)>=0);
    if(ok) ok = ExecutiveSeleToObject(TempPyMOLGlobals,str1,s1,
                                      source,target,discrete,zoom,quiet,
                                      singletons); 
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);
}


static PyObject *CmdOrient(PyObject *dummy, PyObject *args)
{
  double m[16];
  char *str1;
  OrthoLineType s1;
  int state;
  int ok=false;
  float animate;
  int quiet=false; /* TODO */
  ok = PyArg_ParseTuple(args,"sif",&str1,&state,&animate);
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    if(ExecutiveGetMoment(TempPyMOLGlobals,s1,m,state))
      ExecutiveOrient(TempPyMOLGlobals,s1,m,state,animate,false,0.0F,quiet); /* TODO STATUS */
    SelectorFreeTmp(TempPyMOLGlobals,s1); 
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdFitPairs(PyObject *dummy, PyObject *args)
{
  PyObject *list;
  WordType *word = NULL;
  int ln=0;
  int a;
  PyObject *result = NULL;
  float valu;
  int ok=false;
  ok = PyArg_ParseTuple(args,"O",&list);
  if(ok) {
    APIEnterBlocked();
    ln = PyObject_Length(list);
    if(ln) {
      if(ln&0x1)
        ok=ErrMessage(TempPyMOLGlobals,"FitPairs","must supply an even number of selections.");
    } else ok=false;
    
    if(ok) {
      word = Alloc(WordType,ln);
      
      a=0;
      while(a<ln) {
        SelectorGetTmp(TempPyMOLGlobals,PyString_AsString(PySequence_GetItem(list,a)),word[a]);
        a++;
      }
      APIEntry();
      valu = ExecutiveRMSPairs(TempPyMOLGlobals,word,ln/2,2);
      APIExit();
      result=Py_BuildValue("f",valu);
      for(a=0;a<ln;a++)
        SelectorFreeTmp(TempPyMOLGlobals,word[a]);
      FreeP(word);
    }
    APIExitBlocked();
  }
  return APIAutoNone(result);
}

static PyObject *CmdIntraFit(PyObject *dummy, PyObject *args)
{
  char *str1;
  int state;
  int mode;
  int quiet;
  int mix;
  OrthoLineType s1;
  float *fVLA;
  PyObject *result=Py_None;
  int ok=false;
  ok = PyArg_ParseTuple(args,"siiii",&str1,&state,&mode,&quiet,&mix);
  if(state<0) state=0;
  if (ok) {
    APIEntry();
    ok=(SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    fVLA=ExecutiveRMSStates(TempPyMOLGlobals,s1,state,mode,quiet,mix);
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
    if(fVLA) {
      result=PConvFloatVLAToPyList(fVLA);
      VLAFreeP(fVLA);
    }
  }
  return APIAutoNone(result);
}

static PyObject *CmdGetAtomCoords(PyObject *dummy, PyObject *args)
{
  char *str1;
  int state;
  int quiet;
  OrthoLineType s1;
  float vertex[3];
  PyObject *result=Py_None;
  int ok=false;
  ok = PyArg_ParseTuple(args,"sii",&str1,&state,&quiet);
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    if(ok) ok=ExecutiveGetAtomVertex(TempPyMOLGlobals,s1,state,quiet,vertex);
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
    if(ok) {
      result=PConvFloatArrayToPyList(vertex,3);
    }
  }
  return APIAutoNone(result);
}

static PyObject *CmdFit(PyObject *dummy, PyObject *args)
{
  char *str1,*str2;
  int mode;
  int quiet;
  OrthoLineType s1,s2;
  PyObject *result;
  float cutoff;
  int state1,state2;
  int ok=false;
  int matchmaker,cycles;
  char *object;
  ExecutiveRMSInfo rms_info;
  ok = PyArg_ParseTuple(args,"ssiiiiifis",&str1,&str2,&mode,
                        &state1,&state2,&quiet,&matchmaker,&cutoff,&cycles,&object);
  if (ok) {
    APIEntry();
    ok = ((SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0) &&
          (SelectorGetTmp(TempPyMOLGlobals,str2,s2)>=0));
    if(ok) ok = ExecutiveRMS(TempPyMOLGlobals,s1,s2,mode,
                             cutoff,cycles,quiet,object,state1,state2,
                             false, matchmaker, &rms_info);
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    SelectorFreeTmp(TempPyMOLGlobals,s2);
    APIExit();
  }
  if(ok) {
    result=Py_BuildValue("f",rms_info.final_rms);
  } else {
    result=Py_BuildValue("f",-1.0F);
  }
  return result;
}

static PyObject *CmdUpdate(PyObject *dummy, PyObject *args)
{
  char *str1,*str2;
  int int1,int2;
  OrthoLineType s1,s2;
  int ok=false;
  int matchmaker,quiet;
  ok = PyArg_ParseTuple(args,"ssiiii",&str1,&str2,&int1,&int2,&matchmaker,&quiet);
  if (ok) {
    APIEntry();
    ok = ((SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0) &&
          (SelectorGetTmp(TempPyMOLGlobals,str2,s2)>=0));
    ExecutiveUpdateCmd(TempPyMOLGlobals,s1,s2,int1,int2,matchmaker,quiet); /* TODO STATUS */
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    SelectorFreeTmp(TempPyMOLGlobals,s2);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdDirty(PyObject *self, 	PyObject *args)
{
  PRINTFD(TempPyMOLGlobals,FB_CCmd)
    " CmdDirty: called.\n"
    ENDFD;
  APIEntry();
  OrthoDirty(TempPyMOLGlobals);
  APIExit();
  return APISuccess();
}

static PyObject *CmdGetObjectList(PyObject *self, 	PyObject *args)
{
  char *str1;
  OrthoLineType s1;
  int ok=false;
  ObjectMolecule **list = NULL;
  PyObject *result=NULL;

  ok = PyArg_ParseTuple(args,"s",&str1);
  
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    list = ExecutiveGetObjectMoleculeVLA(TempPyMOLGlobals, s1);
    if(list) {
      unsigned int size = VLAGetSize(list);
      result = PyList_New(size);
      if(result) {
        unsigned int a;
        for(a=0;a<size;a++) {
          PyList_SetItem(result,a,PyString_FromString(list[a]->Obj.Name));
        }
      }
      VLAFreeP(list);
    }
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return(APIAutoNone(result));
}

static PyObject *CmdGetDistance(PyObject *self, 	PyObject *args)
{
  char *str1,*str2;
  float result;
  int int1;
  OrthoLineType s1,s2;
  int ok=false;
  ok = PyArg_ParseTuple(args,"ssi",&str1,&str2,&int1);
  
  if (ok) {
    APIEntry();
    ok = ((SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0) &&
          (SelectorGetTmp(TempPyMOLGlobals,str2,s2)>=0));
    if(ok) ok = ExecutiveGetDistance(TempPyMOLGlobals,s1,s2,&result,int1);
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    SelectorFreeTmp(TempPyMOLGlobals,s2);
    APIExit();
  }
  
  if(ok) {
    return(Py_BuildValue("f",result));
  } else {
    return APIFailure();
  }
}


static PyObject *CmdGetAngle(PyObject *self, 	PyObject *args)
{
  char *str1,*str2,*str3;
  float result;
  int int1;
  OrthoLineType s1,s2,s3;
  int ok=false;
  ok = PyArg_ParseTuple(args,"sssi",&str1,&str2,&str3,&int1);
  
  if (ok) {
    APIEntry();
    ok = ((SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0) &&
          (SelectorGetTmp(TempPyMOLGlobals,str2,s2)>=0) &&
          (SelectorGetTmp(TempPyMOLGlobals,str3,s3)>=0));
    if(ok) ok = ExecutiveGetAngle(TempPyMOLGlobals,s1,s2,s3,&result,int1);
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    SelectorFreeTmp(TempPyMOLGlobals,s2);
    SelectorFreeTmp(TempPyMOLGlobals,s3);
    APIExit();
  }
  
  if(ok) {
    return(Py_BuildValue("f",result));
  } else {
    return APIFailure();
  }
}

static PyObject *CmdGetDihe(PyObject *self, 	PyObject *args)
{
  char *str1,*str2,*str3,*str4;
  float result;
  int int1;
  OrthoLineType s1,s2,s3,s4;
  int ok=false;
  ok = PyArg_ParseTuple(args,"ssssi",&str1,&str2,&str3,&str4,&int1);
  
  if (ok) {
    APIEntry();
    ok = ((SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0) &&
          (SelectorGetTmp(TempPyMOLGlobals,str2,s2)>=0) &&
          (SelectorGetTmp(TempPyMOLGlobals,str3,s3)>=0) &&
          (SelectorGetTmp(TempPyMOLGlobals,str4,s4)>=0));
    ok = ExecutiveGetDihe(TempPyMOLGlobals,s1,s2,s3,s4,&result,int1);
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    SelectorFreeTmp(TempPyMOLGlobals,s2);
    SelectorFreeTmp(TempPyMOLGlobals,s3);
    SelectorFreeTmp(TempPyMOLGlobals,s4);
    APIExit();
  }
  
  if(ok) {
    return(Py_BuildValue("f",result));
  } else {
    return APIFailure();
  }
}

static PyObject *CmdSetDihe(PyObject *self, 	PyObject *args)
{
  char *str1,*str2,*str3,*str4;
  float float1;
  int int1;
  int quiet;
  OrthoLineType s1,s2,s3,s4;
  int ok=false;
  ok = PyArg_ParseTuple(args,"ssssfii",&str1,&str2,&str3,&str4,&float1,&int1,&quiet);
  if (ok) {
    APIEntry();
    ok = ((SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0) &&
          (SelectorGetTmp(TempPyMOLGlobals,str2,s2)>=0) &&
          (SelectorGetTmp(TempPyMOLGlobals,str3,s3)>=0) &&
          (SelectorGetTmp(TempPyMOLGlobals,str4,s4)>=0));
    ok = ExecutiveSetDihe(TempPyMOLGlobals,s1,s2,s3,s4,float1,int1,quiet);
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    SelectorFreeTmp(TempPyMOLGlobals,s2);
    SelectorFreeTmp(TempPyMOLGlobals,s3);
    SelectorFreeTmp(TempPyMOLGlobals,s4);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdDo(PyObject *self, 	PyObject *args)
{
  char *str1;
  int log;
  int ok=false;
  int echo;
  ok = PyArg_ParseTuple(args,"sii",&str1,&log,&echo);
  if (ok) {
    APIEntry();
    if(str1[0]!='_') { /* suppress internal call-backs */
      if(strncmp(str1,"cmd._",5)&&(strncmp(str1,"_cmd.",5))) {
        if(echo) {
          OrthoAddOutput(TempPyMOLGlobals,"PyMOL>");
          OrthoAddOutput(TempPyMOLGlobals,str1);
          OrthoNewLine(TempPyMOLGlobals,NULL,true);
        }
        if(log) 
          if(WordMatch(TempPyMOLGlobals,str1,"quit",true)==0) /* don't log quit */
            PLog(str1,cPLog_pml);
      }
      PParse(str1);
    } else if(str1[1]==' ') { 
      /* "_ command" suppresses echoing of command, but it is still logged */
      if(log)
        if(WordMatch(TempPyMOLGlobals,str1+2,"quit",true)==0) /* don't log quit */
          PLog(str1+2,cPLog_pml);
      PParse(str1+2);    
    } else {
      PParse(str1);
    }
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdRock(PyObject *self, PyObject *args)
{
  int int1;
  int result = -1;
  if(PyArg_ParseTuple(args,"i",&int1)) {
    APIEntry();
    result = ControlRock(TempPyMOLGlobals,int1);
    APIExit();
  }
  return APIResultCode(result);
}

static PyObject *CmdBusyDraw(PyObject *self, PyObject *args)
{
  int int1;
  int ok=true;
  ok = PyArg_ParseTuple(args,"i",&int1);
  APIEntry();
  OrthoBusyDraw(TempPyMOLGlobals,int1);
  APIExit();
  return APIResultOk(ok);
}

static PyObject *CmdSetBusy(PyObject *self, PyObject *args)
{
  int int1;
  int ok=true;
  ok = PyArg_ParseTuple(args,"i",&int1);
  PLockStatus();
 PyMOL_SetBusy(TempPyMOLGlobals->PyMOL,int1);
  PUnlockStatus();
  return APIResultOk(ok);
}

static PyObject *CmdGetBusy(PyObject *self, PyObject *args)
{
  int result;
  int ok=true;
  int int1;
  ok = PyArg_ParseTuple(args,"i",&int1);
  PLockStatus();
  result = PyMOL_GetBusy(TempPyMOLGlobals->PyMOL,int1);
  PUnlockStatus();
  return(APIResultCode(result));
}

static PyObject *CmdGetProgress(PyObject *self, PyObject *args)
{
  if(TempPyMOLGlobals && TempPyMOLGlobals->Ready && 
     (!SettingGetGlobal_b(TempPyMOLGlobals,cSetting_sculpting))) {
    
    /* assumes status is already locked */
    
    float result = -1.0F;
    float value=0.0F, range=1.0F;
    int ok=true;
    int int1;
    int offset;
    int progress[PYMOL_PROGRESS_SIZE];
    
    ok = PyArg_ParseTuple(args,"i",&int1);
    if(PyMOL_GetBusy(TempPyMOLGlobals->PyMOL,false)) {
      PyMOL_GetProgress(TempPyMOLGlobals->PyMOL,progress,false);
      
      for(offset=PYMOL_PROGRESS_FAST;offset>=PYMOL_PROGRESS_SLOW;offset-=2) {
        if(progress[offset+1]) {
          float old_value = value;
          float old_range = range;
          
          range = (float)(progress[PYMOL_PROGRESS_FAST+1]);
          value = (float)(progress[PYMOL_PROGRESS_FAST]);
          
          value += (1.0F/range)*(old_value/old_range);
          
          result = value/range;
        }
      }
    }
    return(PyFloat_FromDouble((double)result));
  }
  else
    return(PyFloat_FromDouble(-1.0));
}


static PyObject *CmdGetMoment(PyObject *self, 	PyObject *args) /* missing? */
{
  double moment[16];
  PyObject *result;
  char *str1;
  int ok=false;
  int state;

  ok = PyArg_ParseTuple(args,"si",&str1,&state);
  if (ok) {
    APIEntry();
    ExecutiveGetMoment(TempPyMOLGlobals,str1,moment,state);
    APIExit();
  }
  result = Py_BuildValue("(ddd)(ddd)(ddd)", 
								 moment[0],moment[1],moment[2],
								 moment[3],moment[4],moment[5],
								 moment[6],moment[7],moment[8]);

  return result;
}

static PyObject *CmdGetSetting(PyObject *self, 	PyObject *args)
{
  PyObject *result = Py_None;
  char *str1;
  float value;
  int ok=false;
  ok = PyArg_ParseTuple(args,"s",&str1);
  if (ok) {
    APIEnterBlocked();
    value=SettingGetNamed(TempPyMOLGlobals,str1);
    APIExitBlocked();
    result = Py_BuildValue("f", value);
  }
  return APIAutoNone(result);
}

static PyObject *CmdGetSettingTuple(PyObject *self, 	PyObject *args)
{
  PyObject *result = Py_None;
  int int1,int2;
  char *str1;
  int ok = false;
  ok = PyArg_ParseTuple(args,"isi",&int1,&str1,&int2); /* setting, object, state */
  if (ok) {
    APIEnterBlocked();
    result =  ExecutiveGetSettingTuple(TempPyMOLGlobals,int1,str1,int2);
    APIExitBlocked();
  }
  return APIAutoNone(result);
}

static PyObject *CmdGetSettingOfType(PyObject *self, 	PyObject *args)
{
  PyObject *result = Py_None;
  int int1,int2,int3;
  char *str1;
  int ok=false;
  ok = PyArg_ParseTuple(args,"isii",&int1,&str1,&int2,&int3); /* setting, object, state */
  if (ok) {
    APIEnterBlocked();
    result =  ExecutiveGetSettingOfType(TempPyMOLGlobals,int1,str1,int2,int3);
    APIExitBlocked();
  }
  return APIAutoNone(result);
}

static PyObject *CmdGetSettingText(PyObject *self, 	PyObject *args)
{
  PyObject *result = Py_None;
  int int1,int2;
  char *str1;
  int ok=false;
  ok = PyArg_ParseTuple(args,"isi",&int1,&str1,&int2); /* setting, object, state */
  if (ok) {
    APIEnterBlocked();
    result =  ExecutiveGetSettingText(TempPyMOLGlobals,int1,str1,int2);
    APIExitBlocked();
  }
  return APIAutoNone(result);
}

static PyObject *CmdExportDots(PyObject *self, 	PyObject *args)
{
  PyObject *result=NULL;
  PyObject *cObj;
  ExportDotsObj *obj;
  char *str1;
  int int1;
  
  int ok=false;
  ok = PyArg_ParseTuple(args,"si",&str1,&int1);
  if (ok) {
    APIEntry();
    obj = ExportDots(TempPyMOLGlobals,str1,int1);
    APIExit();
    if(obj) 
      {
        cObj = PyCObject_FromVoidPtr(obj,(void(*)(void*))ExportDeleteMDebug);
        if(cObj) {
          result = Py_BuildValue("O",cObj); /* I think this */
          Py_DECREF(cObj); /* transformation is unnecc. */
        } 
      }
  }
  return APIAutoNone(result);
}

static PyObject *CmdSetFrame(PyObject *self, PyObject *args)
{
  int mode,frm;
  int ok=false;
  ok = PyArg_ParseTuple(args,"ii",&mode,&frm);
  if (ok) {
    APIEntry();
    SceneSetFrame(TempPyMOLGlobals,mode,frm);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdFrame(PyObject *self, PyObject *args)
{
  int frm;
  int ok=false;
  ok = PyArg_ParseTuple(args,"i",&frm);
  frm--;
  if(frm<0) frm=0;
  if (ok) {
    APIEntry();
    SceneSetFrame(TempPyMOLGlobals,4,frm);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdStereo(PyObject *self, PyObject *args)
{
  int i1;
  int ok=false;
  
  ok = PyArg_ParseTuple(args,"i",&i1);
  if (ok) {
    APIEntry();
    ok = ExecutiveStereo(TempPyMOLGlobals,i1); 
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdReset(PyObject *self, PyObject *args)
{
  int cmd;
  int ok=false;
  char *obj;
  ok = PyArg_ParseTuple(args,"is",&cmd,&obj);
  if (ok) {
    APIEntry();
    ok = ExecutiveReset(TempPyMOLGlobals,cmd,obj); 
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdSetMatrix(PyObject *self, 	PyObject *args)
{
  float m[16];
  int ok=false;
  ok = PyArg_ParseTuple(args,"ffffffffffffffff",
						 &m[0],&m[1],&m[2],&m[3],
						 &m[4],&m[5],&m[6],&m[7],
						 &m[8],&m[9],&m[10],&m[11],
						 &m[12],&m[13],&m[14],&m[15]);
  if (ok) {
    APIEntry();
    SceneSetMatrix(TempPyMOLGlobals,m); /* TODO STATUS */
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdGetMinMax(PyObject *self, 	PyObject *args)
{
  float mn[3],mx[3];
  char *str1;
  int state;
  OrthoLineType s1;
  PyObject *result = Py_None;
  int flag;

  int ok=false;
  ok = PyArg_ParseTuple(args,"si",&str1,&state); 
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    flag = ExecutiveGetExtent(TempPyMOLGlobals,s1,mn,mx,true,state,false);
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
    if(flag) 
      result = Py_BuildValue("[[fff],[fff]]", 
                             mn[0],mn[1],mn[2],
                             mx[0],mx[1],mx[2]);
    else 
      result = Py_BuildValue("[[fff],[fff]]", 
                             -0.5,-0.5,-0.5,
                             0.5,0.5,0.5);
    
  }
  return APIAutoNone(result);
}

static PyObject *CmdGetMatrix(PyObject *self, 	PyObject *args)
{
  float *f;
  PyObject *result;
  
  APIEntry();
  f=SceneGetMatrix(TempPyMOLGlobals);
  APIExit();
  result = Py_BuildValue("ffffffffffffffff", 
								 f[0],f[1],f[2],f[3],
								 f[4],f[5],f[6],f[7],
								 f[8],f[9],f[10],f[11],
								 f[12],f[13],f[14],f[15]
								 );
  return result;
}

static PyObject *CmdGetObjectMatrix(PyObject *self, 	PyObject *args)
{
  PyObject *result = NULL;
  char *name;
  double *history = NULL;
  int ok=false;
  int found;
  int state;
  ok = PyArg_ParseTuple(args,"si",&name,&state);
  
  APIEntry();
  found = ExecutiveGetObjectMatrix(TempPyMOLGlobals,name,state,&history);
  APIExit();
  if(found) {
    if(history) 
      result = Py_BuildValue("dddddddddddddddd",
                             history[ 0],history[ 1],history[ 2],history[ 3],
                             history[ 4],history[ 5],history[ 6],history[ 7],
                             history[ 8],history[ 9],history[10],history[11],
                             history[12],history[13],history[14],history[15]
                             );
    else 
       result = Py_BuildValue("dddddddddddddddd",
                              1.0, 0.0, 0.0, 0.0,
                              0.0, 1.0, 0.0, 0.0,
                              0.0, 0.0, 1.0, 0.0,
                              0.0, 0.0, 0.0, 1.0);
  }
  return APIAutoNone(result);  
}

static PyObject *CmdMDo(PyObject *self, 	PyObject *args)
{
  char *cmd;
  int frame;
  int append;
  int ok=false;
  ok = PyArg_ParseTuple(args,"isi",&frame,&cmd,&append);
  if (ok) {
    APIEntry();
    if(append) {
      MovieAppendCommand(TempPyMOLGlobals,frame,cmd);
    } else {
      MovieSetCommand(TempPyMOLGlobals,frame,cmd);
    }
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdMPlay(PyObject *self, 	PyObject *args)
{
  int cmd;
  int ok=false;
  ok = PyArg_ParseTuple(args,"i",&cmd);
  if (ok) {
    APIEntry();
    MoviePlay(TempPyMOLGlobals,cmd);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdMMatrix(PyObject *self, 	PyObject *args)
{
  int cmd;
  int ok=false;
  ok = PyArg_ParseTuple(args,"i",&cmd);
  if (ok) {
    APIEntry();
    ok = MovieMatrix(TempPyMOLGlobals,cmd);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdMClear(PyObject *self, 	PyObject *args)
{
  APIEntry();
  MovieClearImages(TempPyMOLGlobals);
  APIExit();
  return APISuccess();
}

static PyObject *CmdRefresh(PyObject *self, 	PyObject *args)
{
  APIEntry();
  SceneInvalidateCopy(TempPyMOLGlobals,false);
  ExecutiveDrawNow(TempPyMOLGlobals); /* TODO STATUS */
  APIExit();
  return APISuccess();
}

static PyObject *CmdRefreshNow(PyObject *self, 	PyObject *args)
{
  APIEntry();
  PyMOL_PushValidContext(TempPyMOLGlobals->PyMOL); /* we're trusting the caller on this... */
  SceneInvalidateCopy(TempPyMOLGlobals,false);
  ExecutiveDrawNow(TempPyMOLGlobals); /* TODO STATUS */
#ifndef _PYMOL_NO_MAIN
  MainRefreshNow();
#endif
  PyMOL_PopValidContext(TempPyMOLGlobals->PyMOL);
  APIExit();
  return APISuccess();
}

static PyObject *CmdPNG(PyObject *self, 	PyObject *args)
{
  char *str1;
  int ok=false;
  int quiet;
  int width,height;
  float dpi;
  ok = PyArg_ParseTuple(args,"siifi",&str1,&width,&height,&dpi,&quiet);
  if (ok) {
    APIEntry();
    ExecutiveDrawNow(TempPyMOLGlobals);		 /* TODO STATUS */
    if(width||height) {
      SceneDeferImage(TempPyMOLGlobals,width,height,str1,-1,dpi,quiet);
    } else {
      ScenePNG(TempPyMOLGlobals,str1,dpi,quiet);
    }
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdMPNG(PyObject *self, 	PyObject *args)
{
  char *str1;
  int int1,int2;
  int ok=false;
  ok = PyArg_ParseTuple(args,"sii",&str1,&int1,&int2);
  if (ok) {
    APIEntry();
    ok = MoviePNG(TempPyMOLGlobals,str1,(int)SettingGet(TempPyMOLGlobals,cSetting_cache_frames),int1,int2);
    /* TODO STATUS */
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdMSet(PyObject *self, 	PyObject *args)
{
  char *str1;
  int ok=false;
  int start_from;
  ok = PyArg_ParseTuple(args,"si",&str1,&start_from);
  if (ok) {
    APIEntry();
    MovieAppendSequence(TempPyMOLGlobals,str1,start_from);
    SceneCountFrames(TempPyMOLGlobals);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdMView(PyObject *self, 	PyObject *args)
{
  int ok=false;
  int action,first,last, simple,wrap,window,cycles;
  float power,bias,linear,hand;
  char *object;
  ok = PyArg_ParseTuple(args,"iiiffifsiiii",&action,&first,&last,&power,
                        &bias,&simple,&linear,&object,&wrap,&hand,
                        &window,&cycles);
  if (ok) {
    APIEntry();
    if(wrap<0) {
      wrap = SettingGetGlobal_b(TempPyMOLGlobals,cSetting_movie_loop);
    }
    if(object[0]) {
      CObject *obj = ExecutiveFindObjectByName(TempPyMOLGlobals,object);
      if(!obj) {
        ok = false;
      } else {
        if(simple<0) simple = 0; 
        ok = ObjectView(obj,action,first,last,power,bias,simple,linear,wrap,hand,window,cycles);
      }
    } else {
      simple = true; /* force this because camera matrix does't work like a TTT */
      ok = MovieView(TempPyMOLGlobals,action,first,last,power,bias,simple,linear,wrap,hand,window,cycles);
    }
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdViewport(PyObject *self, 	PyObject *args)
{
  int w,h;
  int ok=false;
  ok = PyArg_ParseTuple(args,"ii",&w,&h);
  if(ok) {
    if(((w>0)&&(h<=0))||
       ((h>0)&&(w<=0))) {
      int cw,ch;
      SceneGetWidthHeight(TempPyMOLGlobals, &cw, &ch);
      if(h<=0) {
        h = (w * ch) / cw;
      }
      if(w<=0) {
        w = (h * cw) / ch;
      }
    }
    if((w>0)&&(h>0)) {
      if(w<10) w=10;
      if(h<10) h=10;
      if(SettingGet(TempPyMOLGlobals,cSetting_internal_gui)) {
        if(!SettingGet(TempPyMOLGlobals,cSetting_full_screen))
           w+=(int)SettingGet(TempPyMOLGlobals,cSetting_internal_gui_width);
      }
      if(SettingGet(TempPyMOLGlobals,cSetting_internal_feedback)) {
        if(!SettingGet(TempPyMOLGlobals,cSetting_full_screen))
          h+=(int)(SettingGet(TempPyMOLGlobals,cSetting_internal_feedback)-1)*cOrthoLineHeight +
            cOrthoBottomSceneMargin;
      }
    } else {
      w=-1;
      h=-1;
    }
    APIEntry();
#ifndef _PYMOL_NO_MAIN
    MainDoReshape(w,h); /* should be moved into Executive */
#else
    PyMOL_NeedReshape(TempPyMOLGlobals->PyMOL,2,0,0,w,h);
#endif
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdFlag(PyObject *self, 	PyObject *args)
{
  char *str1;
  int flag;
  int action;
  int quiet;
  OrthoLineType s1;
  int ok=false;
  ok = PyArg_ParseTuple(args,"isii",&flag,&str1,&action,&quiet);
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    ExecutiveFlag(TempPyMOLGlobals,flag,s1,action,quiet);
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdColor(PyObject *self, 	PyObject *args)
{
  char *str1,*color;
  int flags;
  OrthoLineType s1;
  int ok=false;
  int quiet;
  ok = PyArg_ParseTuple(args,"ssii",&color,&str1,&flags,&quiet);
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    if(ok) {
      ok = ExecutiveColor(TempPyMOLGlobals,s1,color,flags,quiet);
    }
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdColorDef(PyObject *self, 	PyObject *args)
{
  char *color;
  float v[3];
  int ok=false;
  int mode;
  int quiet;
  ok = PyArg_ParseTuple(args,"sfffii",&color,v,v+1,v+2,&mode,&quiet);
  if (ok) {
    APIEntry();  
    ColorDef(TempPyMOLGlobals,color,v,mode,quiet);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdDraw(PyObject *self, 	PyObject *args)
{
  int int1,int2;
  int quiet,antialias;
  int ok = PyArg_ParseTuple(args,"iiii",&int1,&int2,&antialias,&quiet);
  if(ok) {
    APIEntry();
    ok = ExecutiveDrawCmd(TempPyMOLGlobals,int1,int2,antialias,quiet);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdRay(PyObject *self, 	PyObject *args)
{
  int w,h,mode;
  float angle,shift;
  int ok=false;
  int quiet;
  int antialias;
  ok = PyArg_ParseTuple(args,"iiiffii", &w, &h,
                        &antialias, 
                        &angle, &shift, &mode, &quiet);
  if (ok) {
    APIEntry();
    if(mode<0)
      mode=(int)SettingGet(TempPyMOLGlobals,cSetting_ray_default_renderer);
    ExecutiveRay(TempPyMOLGlobals,w,h,mode,angle,shift,quiet,false,antialias); /* TODO STATUS */
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdClip(PyObject *self, 	PyObject *args)
{
  char *sname;
  float dist;
  char *str1;
  int state;
  OrthoLineType s1;
  int ok=false;
  ok = PyArg_ParseTuple(args,"sfsi",&sname,&dist,&str1,&state);
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    switch(sname[0]) { /* TODO STATUS */
    case 'N':
    case 'n':
      SceneClip(TempPyMOLGlobals,0,dist,s1,state);
      break;
    case 'f':
    case 'F':
      SceneClip(TempPyMOLGlobals,1,dist,s1,state);
      break;
    case 'm':
    case 'M':
      SceneClip(TempPyMOLGlobals,2,dist,s1,state);
      break;
    case 's':
    case 'S':
      SceneClip(TempPyMOLGlobals,3,dist,s1,state);
      break;
    case 'a':
    case 'A':
      SceneClip(TempPyMOLGlobals,4,dist,s1,state);
      break;
    }
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);

}

static PyObject *CmdMove(PyObject *self, 	PyObject *args)
{
  char *sname;
  float dist;
  int ok=false;
  ok = PyArg_ParseTuple(args,"sf",&sname,&dist);
  if (ok) {
    APIEntry();
    switch(sname[0]) { /* TODO STATUS */
    case 'x':
      SceneTranslate(TempPyMOLGlobals,dist,0.0,0.0);
      break;
    case 'y':
      SceneTranslate(TempPyMOLGlobals,0.0,dist,0.0);
      break;
    case 'z':
      SceneTranslate(TempPyMOLGlobals,0.0,0.0,dist);
      break;
    }
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdTurn(PyObject *self, 	PyObject *args)
{
  char *sname;
  float angle;
  int ok=false;
  ok = PyArg_ParseTuple(args,"sf",&sname,&angle);
  if (ok) {
    APIEntry();
    switch(sname[0]) { /* TODO STATUS */
    case 'x':
      SceneRotate(TempPyMOLGlobals,angle,1.0,0.0,0.0);
      break;
    case 'y':
      SceneRotate(TempPyMOLGlobals,angle,0.0,1.0,0.0);
      break;
    case 'z':
      SceneRotate(TempPyMOLGlobals,angle,0.0,0.0,1.0);
      break;
    }
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdLegacySet(PyObject *self, 	PyObject *args)
{
  char *sname, *value;
  int ok=false;
  ok = PyArg_ParseTuple(args,"ss",&sname,&value);
  if (ok) {
    APIEntry();
    ok = SettingSetNamed(TempPyMOLGlobals,sname,value); 
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdUnset(PyObject *self, 	PyObject *args)
{
  int index;
  int tmpFlag=false;
  char *str3;
  int state;
  int quiet;
  int updates;
  OrthoLineType s1;
  int ok=false;
  ok = PyArg_ParseTuple(args,"isiii",&index,&str3,&state,&quiet,&updates);
  s1[0]=0;
  if (ok) {
    APIEntry();
    if(!strcmp(str3,"all")) {
      strcpy(s1,str3);
    } else if(str3[0]!=0) {
      tmpFlag=true;
      ok = (SelectorGetTmp(TempPyMOLGlobals,str3,s1)>=0);
    }
    if(ok) ok = ExecutiveUnsetSetting(TempPyMOLGlobals,index,s1,state,quiet,updates);
    if(tmpFlag) 
      SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);
}
#if 0
static PyObject *CmdUnsetBond(PyObject *self, 	PyObject *args)
{
  int index;
  int tmpFlag=false;
  char *str3,*str4;
  int state;
  int quiet;
  int updates;
  OrthoLineType s1,s2;
  int ok=false;
  ok = PyArg_ParseTuple(args,"issiii",&index,&str3,&str4,&state,&quiet,&updates);
  s1[0]=0;
  if (ok) {
    APIEntry();
    if(!strcmp(str3,"all")) {
      strcpy(s1,str3);
    } else if(str3[0]!=0) {
      tmpFlag=true;
      ok = (SelectorGetTmp(TempPyMOLGlobals,str3,s1)>=0);
    }
    if(ok) ok = ExecutiveUnsetSetting(TempPyMOLGlobals,index,s1,state,quiet,updates);
    if(tmpFlag) 
      SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);
}
#endif

static PyObject *CmdSet(PyObject *self, 	PyObject *args)
{
  int index;
  int tmpFlag=false;
  PyObject *value;
  char *str3;
  int state;
  int quiet;
  int updates;
  OrthoLineType s1;
  int ok=false;
  ok = PyArg_ParseTuple(args,"iOsiii",&index,&value,&str3,&state,&quiet,&updates);
  s1[0]=0;
  if (ok) {
    APIEntry();
    if(!strcmp(str3,"all")) {
      strcpy(s1,str3);
    } else if(str3[0]!=0) {
      tmpFlag=true;
      ok = (SelectorGetTmp(TempPyMOLGlobals,str3,s1)>=0);
    }
    if(ok) ok = ExecutiveSetSetting(TempPyMOLGlobals,index,value,s1,state,quiet,updates);
    if(tmpFlag) 
      SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);
}
#if 0
static PyObject *CmdSetBond(PyObject *self, 	PyObject *args)
{
  int index;
  int tmpFlag=false;
  PyObject *value;
  char *str3,*str4;
  int state;
  int quiet;
  int updates;
  OrthoLineType s1,s2;
  int ok=false;
  ok = PyArg_ParseTuple(args,"iOssiii",&index,&value,&str3,&str4,&state,&quiet,&updates);
  s1[0]=0;
  s2[0]=0;
  if (ok) {
    APIEntry();
    if(!strcmp(str3,"all")) {
      strcpy(s1,str3);
    } else if(str3[0]!=0) {
      tmpFlag=true;
      ok = (SelectorGetTmp(TempPyMOLGlobals,str3,s1)>=0);
    }
    if(!strcmp(str4,"all")) {
      strcpy(s1,str4);
    } else if(str4[0]!=0) {
      tmpFlag=true;
      ok = (SelectorGetTmp(TempPyMOLGlobals,str4,s2)>=0);
    }
    if(ok) ok = ExecutiveSetBondSetting(TempPyMOLGlobals,index,value,s1,s2,state,quiet,updates);
    if(tmpFlag) 
      SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);
}
#endif
static PyObject *CmdGet(PyObject *self, 	PyObject *args)
{
  float f;
  char *sname;
  PyObject *result = Py_None;

  int ok=false;
  ok = PyArg_ParseTuple(args,"s",&sname);
  if (ok) {
    APIEnterBlocked();
    f=SettingGetNamed(TempPyMOLGlobals,sname);
    APIExitBlocked();
    result = Py_BuildValue("f", f);
  }
  return APIAutoNone(result);
}

static PyObject *CmdDelete(PyObject *self, 	PyObject *args)
{
  char *sname;

  int ok=false;
  ok = PyArg_ParseTuple(args,"s",&sname);
  if (ok) {
    APIEntry();
    ExecutiveDelete(TempPyMOLGlobals,sname); /* TODO STATUS */
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdCartoon(PyObject *self, 	PyObject *args)
{
  char *sname;
  int type;
  OrthoLineType s1;
  int ok=false;
  ok = PyArg_ParseTuple(args,"si",&sname,&type);
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,sname,s1)>=0);
    if(ok) ExecutiveCartoon(TempPyMOLGlobals,type,s1); /* TODO STATUS */
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdShowHide(PyObject *self, 	PyObject *args)
{
  char *sname;
  int rep;
  int state;
  OrthoLineType s1;
  int ok=false;
  ok = PyArg_ParseTuple(args,"sii",&sname,&rep,&state);
  if (ok) { /* TODO STATUS */
    APIEntry();
    if(sname[0]=='@') {
      ExecutiveSetAllVisib(TempPyMOLGlobals,state);
    } else {
      ok = (SelectorGetTmp(TempPyMOLGlobals,sname,s1)>=0);
      ExecutiveSetRepVisib(TempPyMOLGlobals,s1,rep,state);
      SelectorFreeTmp(TempPyMOLGlobals,s1);
    }
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdOnOffBySele(PyObject *self, 	PyObject *args)
{
  char *sname;
  int onoff;
  OrthoLineType s1;
  int ok=false;
  ok = PyArg_ParseTuple(args,"si",&sname,&onoff);
  if (ok) { /* TODO STATUS */
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,sname,s1)>=0);
    if(ok) ok = ExecutiveSetOnOffBySele(TempPyMOLGlobals,s1,onoff);
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdOnOff(PyObject *self, 	PyObject *args)
{
  char *name;
  int state;
  int ok=false;
  ok = PyArg_ParseTuple(args,"si",&name,&state);
  if (ok) { /* TODO STATUS */
    APIEntry();
    ExecutiveSetObjVisib(TempPyMOLGlobals,name,state);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdToggle(PyObject *self, 	PyObject *args)
{
  char *sname;
  int rep;
  OrthoLineType s1;
  int ok=false;
  ok = PyArg_ParseTuple(args,"si",&sname,&rep);
  if (ok) {
    APIEntry();
    if(sname[0]=='@') {
      /* TODO */
    } else {
      ok = (SelectorGetTmp(TempPyMOLGlobals,sname,s1)>=0);
      if(ok) ok = ExecutiveToggleRepVisib(TempPyMOLGlobals,s1,rep);
      SelectorFreeTmp(TempPyMOLGlobals,s1);
    }
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdQuit(PyObject *self, 	PyObject *args)
{
  APIEntry();
  TempPyMOLGlobals->Terminating=true;
  PExit(EXIT_SUCCESS);
  APIExit();
  return APISuccess();
}
static PyObject *CmdFullScreen(PyObject *self,PyObject *args)
{
  int flag = 0;
  int ok=false;
  ok = PyArg_ParseTuple(args,"i",&flag);
  if (ok) {
    APIEntry();
    ExecutiveFullScreen(TempPyMOLGlobals,flag); /* TODO STATUS */
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdSelect(PyObject *self, PyObject *args)
{
  char *sname,*sele;
  int quiet;
  int ok=false;
  int count = 0;
  ok = PyArg_ParseTuple(args,"ssi",&sname,&sele,&quiet);
  if(ok) {
    APIEntry();
    count = SelectorCreate(TempPyMOLGlobals,sname,sele,NULL,quiet,NULL);
    if(count<0)
      ok = false;
    SceneInvalidate(TempPyMOLGlobals);
    SeqDirty(TempPyMOLGlobals);
    APIExit();
  }
  return ok ? APIResultCode(count) : APIFailure();
}

static PyObject *CmdFinishObject(PyObject *self, PyObject *args)
{
  char *oname;
  CObject *origObj = NULL;

  int ok=false;
  ok = PyArg_ParseTuple(args,"s",&oname);

  if (ok) {
    APIEntry();
    origObj=ExecutiveFindObjectByName(TempPyMOLGlobals,oname);
    if(origObj) {
      if(origObj->type==cObjectMolecule) {
        ObjectMoleculeUpdateIDNumbers((ObjectMolecule*)origObj);
        ObjectMoleculeUpdateNonbonded((ObjectMolecule*)origObj);
        ObjectMoleculeInvalidate((ObjectMolecule*)origObj,cRepAll,cRepInvAll,-1);
      }
      ExecutiveUpdateObjectSelection(TempPyMOLGlobals,origObj); /* TODO STATUS */
    }
    else
      ok=false;
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdLoadObject(PyObject *self, PyObject *args)
{
  char *oname;
  PyObject *model;
  CObject *origObj = NULL,*obj;
  OrthoLineType buf;
  int frame,type;
  int finish,discrete;
  int quiet;
  int ok=false;
  int zoom;
  ok = PyArg_ParseTuple(args,"sOiiiiii",&oname,&model,&frame,&type,
                        &finish,&discrete,&quiet,&zoom);
  buf[0]=0;
  if (ok) {
    APIEntry();
    origObj=ExecutiveFindObjectByName(TempPyMOLGlobals,oname);
    
    /* TODO check for existing object of wrong type */
    
    switch(type) {
    case cLoadTypeChemPyModel:
      if(origObj)
        if(origObj->type!=cObjectMolecule) {
          ExecutiveDelete(TempPyMOLGlobals,oname);
          origObj=NULL;
        }
      PBlock(); /*PBlockAndUnlockAPI();*/
      obj=(CObject*)ObjectMoleculeLoadChemPyModel(TempPyMOLGlobals,(ObjectMolecule*)
                                                 origObj,model,frame,discrete);
      PUnblock(); /*PLockAPIAndUnblock();*/
      if(!origObj) {
        if(obj) {
          ObjectSetName(obj,oname);
          ExecutiveManageObject(TempPyMOLGlobals,obj,zoom,quiet);
          if(frame<0)
            frame = ((ObjectMolecule*)obj)->NCSet-1;
          sprintf(buf," CmdLoad: ChemPy-model loaded into object \"%s\", state %d.\n",
                  oname,frame+1);		  
        }
      } else if(origObj) {
        if(finish)
          ExecutiveUpdateObjectSelection(TempPyMOLGlobals,origObj);
        if(frame<0)
          frame = ((ObjectMolecule*)origObj)->NCSet-1;
        sprintf(buf," CmdLoad: ChemPy-model appended into object \"%s\", state %d.\n",
                oname,frame+1);
      }
      break;
    case cLoadTypeChemPyBrick:
      if(origObj)
        if(origObj->type!=cObjectMap) {
          ExecutiveDelete(TempPyMOLGlobals,oname);
          origObj=NULL;
        }
      PBlock(); /*PBlockAndUnlockAPI();*/
      obj=(CObject*)ObjectMapLoadChemPyBrick(TempPyMOLGlobals,(ObjectMap*)origObj,model,frame,discrete);
      PUnblock(); /*PLockAPIAndUnblock();*/
      if(!origObj) {
        if(obj) {
          ObjectSetName(obj,oname);
          ExecutiveManageObject(TempPyMOLGlobals,obj,zoom,quiet);
          sprintf(buf," CmdLoad: chempy.brick loaded into object \"%s\"\n",
                  oname);		  
        }
      } else if(origObj) {
        sprintf(buf," CmdLoad: chempy.brick appended into object \"%s\"\n",
                oname);
      }
      break;
    case cLoadTypeChemPyMap:
      if(origObj)
        if(origObj->type!=cObjectMap) {
          ExecutiveDelete(TempPyMOLGlobals,oname);
          origObj=NULL;
        }
      PBlock(); /*PBlockAndUnlockAPI();*/
      obj=(CObject*)ObjectMapLoadChemPyMap(TempPyMOLGlobals,(ObjectMap*)origObj,model,frame,discrete);
      PUnblock(); /*PLockAPIAndUnblock();*/
      if(!origObj) {
        if(obj) {
          ObjectSetName(obj,oname);
          ExecutiveManageObject(TempPyMOLGlobals,obj,zoom,quiet);
          sprintf(buf," CmdLoad: chempy.map loaded into object \"%s\"\n",
                  oname);		  
        }
      } else if(origObj) {
        sprintf(buf," CmdLoad: chempy.map appended into object \"%s\"\n",
                oname);
      }
      break;
    case cLoadTypeCallback:
      if(origObj)
        if(origObj->type!=cObjectCallback) {
          ExecutiveDelete(TempPyMOLGlobals,oname);
          origObj=NULL;
        }
      PBlock(); /*PBlockAndUnlockAPI();*/
      obj=(CObject*)ObjectCallbackDefine(TempPyMOLGlobals,(ObjectCallback*)origObj,model,frame);
      PUnblock(); /*PLockAPIAndUnblock();*/
      if(!origObj) {
        if(obj) {
          ObjectSetName(obj,oname);
          ExecutiveManageObject(TempPyMOLGlobals,obj,zoom,quiet);
          sprintf(buf," CmdLoad: pymol.callback loaded into object \"%s\"\n",
                  oname);		  
        }
      } else if(origObj) {
        sprintf(buf," CmdLoad: pymol.callback appended into object \"%s\"\n",
                oname);
      }
      break;
    case cLoadTypeCGO:
      if(origObj)
        if(origObj->type!=cObjectCGO) {
          ExecutiveDelete(TempPyMOLGlobals,oname);
          origObj=NULL;
        }
      PBlock(); /*PBlockAndUnlockAPI();*/
      obj=(CObject*)ObjectCGODefine(TempPyMOLGlobals,(ObjectCGO*)origObj,model,frame);
      PUnblock(); /*PLockAPIAndUnblock();*/
      if(!origObj) {
        if(obj) {
          ObjectSetName(obj,oname);
          ExecutiveManageObject(TempPyMOLGlobals,obj,zoom,quiet);
          sprintf(buf," CmdLoad: CGO loaded into object \"%s\"\n",
                  oname);		  
        }
      } else if(origObj) {
        sprintf(buf," CmdLoad: CGO appended into object \"%s\"\n",
                oname);
      }
      break;
      
    }
    if(origObj&&!quiet) {
      PRINTFB(TempPyMOLGlobals,FB_Executive,FB_Actions) 
        "%s",buf
        ENDFB(TempPyMOLGlobals);
      OrthoRestorePrompt(TempPyMOLGlobals);
    }
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdLoadCoords(PyObject *self, PyObject *args)
{
  char *oname;
  PyObject *model;
  CObject *origObj = NULL,*obj;
  OrthoLineType buf;
  int frame,type;
  int ok=false;

  buf[0]=0;

  ok = PyArg_ParseTuple(args,"sOii",&oname,&model,&frame,&type);

  if (ok) {
    APIEntry();
    origObj=ExecutiveFindObjectByName(TempPyMOLGlobals,oname);
    
    /* TODO check for existing object of wrong type */
    if(!origObj) {
      ErrMessage(TempPyMOLGlobals,"LoadCoords","named object not found.");
      ok=false;
    } else 
      {
        switch(type) {
        case cLoadTypeChemPyModel:
          PBlock(); /*PBlockAndUnlockAPI();*/
          obj=(CObject*)ObjectMoleculeLoadCoords(TempPyMOLGlobals,(ObjectMolecule*)origObj,model,frame);
          PUnblock(); /*PLockAPIAndUnblock();*/
          if(frame<0)
            frame=((ObjectMolecule*)obj)->NCSet-1;
          sprintf(buf," CmdLoad: Coordinates appended into object \"%s\", state %d.\n",
                  oname,frame+1);
          break;
        }
      }
    if(origObj) {
      PRINTFB(TempPyMOLGlobals,FB_Executive,FB_Actions) 
        "%s",buf
        ENDFB(TempPyMOLGlobals);
      OrthoRestorePrompt(TempPyMOLGlobals);
    }
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdLoad(PyObject *self, PyObject *args)
{
  char *fname,*oname;
  CObject *origObj = NULL,*obj;
  OrthoLineType buf;
  int frame,type;
  int finish,discrete;
  int quiet;
  int ok=false;
  int multiplex;
  int zoom;
  int bytes;
  ok = PyArg_ParseTuple(args,"ss#iiiiiii",
                        &oname,&fname,&bytes,&frame,&type,
                        &finish,&discrete,&quiet,
                        &multiplex,&zoom);
  buf[0]=0;
  PRINTFD(TempPyMOLGlobals,FB_CCmd)
    "CmdLoad-DEBUG %s %s %d %d %d %d\n",
    oname,fname,frame,type,finish,discrete
    ENDFD;
  if (ok) {
    APIEntry();
    if(multiplex==-2) /* use setting default value */
      multiplex = SettingGetGlobal_i(TempPyMOLGlobals,cSetting_multiplex);
    if(multiplex<0) /* default behavior is not to multiplex */
      multiplex = 0;

    if(discrete<0) {/* use default discrete behavior for the file format 
                     * this will be the case for MOL2 and SDF */ 
      if(multiplex==1) /* if also multiplexing, then default discrete
                        * behavior is not load as discrete objects */
        discrete=0;
      else {
        switch(type) {
        case cLoadTypeSDF2: /* SDF files currently default to discrete */
        case cLoadTypeSDF2Str:
          break;
        case cLoadTypeMOL2:
        case cLoadTypeMOL2Str:
          discrete = -1;  /* content-dependent behavior...*/
          break;
        default:
          discrete = 0;
          break;
        }
      }
    }

    if(multiplex!=1)
      origObj = ExecutiveGetExistingCompatible(TempPyMOLGlobals,oname,type);
    
    switch(type) {
    case cLoadTypePDB:
      ok = ExecutiveProcessPDBFile(TempPyMOLGlobals,origObj,fname,oname,
                              frame,discrete,finish,buf,NULL,quiet,
                              false,multiplex,zoom);
      break;
    case cLoadTypePQR:
      {
        PDBInfoRec pdb_info;
        UtilZeroMem(&pdb_info,sizeof(PDBInfoRec));

        pdb_info.is_pqr_file = true;        
        ok = ExecutiveProcessPDBFile(TempPyMOLGlobals,origObj,fname,oname,
                                frame,discrete,finish,buf,&pdb_info,
                                quiet,false,multiplex,zoom);
      }
      break;
    case cLoadTypeTOP:
      PRINTFD(TempPyMOLGlobals,FB_CCmd) " CmdLoad-DEBUG: loading TOP\n" ENDFD;
      if(origObj) { /* always reinitialize topology objects from scratch */
        ExecutiveDelete(TempPyMOLGlobals,origObj->Name);
        origObj=NULL;
      }
      if(!origObj) {
        obj=(CObject*)ObjectMoleculeLoadTOPFile(TempPyMOLGlobals,NULL,fname,frame,discrete);
        if(obj) {
          ObjectSetName(obj,oname);
          ExecutiveManageObject(TempPyMOLGlobals,obj,false,true);
          if(frame<0)
            frame = ((ObjectMolecule*)obj)->NCSet-1;
          sprintf(buf," CmdLoad: \"%s\" loaded as \"%s\".\n",
                  fname,oname);
        }
      }
      break;
    case cLoadTypeXTC:
    case cLoadTypeTRR:
    case cLoadTypeGRO:
    case cLoadTypeG96:
    case cLoadTypeTRJ2:
    case cLoadTypeDCD:
      {
        char *plugin = NULL;
        char xtc[] = "xtc", trr[] = "trr", gro[] = "gro", g96[] = "g96", trj[] = "trj", dcd[] = "dcd";
        switch(type) {
        case cLoadTypeXTC: plugin=xtc; break;
        case cLoadTypeTRR: plugin=trr; break;
        case cLoadTypeGRO: plugin=gro; break;
        case cLoadTypeG96: plugin=g96; break;
        case cLoadTypeTRJ2: plugin=trj; break;
        case cLoadTypeDCD: plugin=dcd; break;
        }
        if(plugin) {
          if(origObj) { /* always reinitialize topology objects from scratch */
            PlugIOManagerLoadTraj(TempPyMOLGlobals,(ObjectMolecule*)origObj,fname,frame,
                                  1,1,1,-1,-1,NULL,1,NULL,quiet,plugin);
            /* if(finish)
               ExecutiveUpdateObjectSelection(TempPyMOLGlobals,origObj); unnecc */
            sprintf(buf," CmdLoad: \"%s\" appended into object \"%s\".\n CmdLoad: %d total states in the object.\n",
                    fname,oname,((ObjectMolecule*)origObj)->NCSet);
          } else {
            PRINTFB(TempPyMOLGlobals,FB_CCmd,FB_Errors)
              "CmdLoad-Error: must load object topology before loading trajectory."
              ENDFB(TempPyMOLGlobals);
          }
        } else {
          PRINTFB(TempPyMOLGlobals,FB_CCmd,FB_Errors)
            "CmdLoad-Error: plugin not found"
            ENDFB(TempPyMOLGlobals);
        }
      }
      break;
    case cLoadTypeTRJ:
      PRINTFD(TempPyMOLGlobals,FB_CCmd) " CmdLoad-DEBUG: loading TRJ\n" ENDFD;
      if(origObj) { /* always reinitialize topology objects from scratch */
        ObjectMoleculeLoadTRJFile(TempPyMOLGlobals,(ObjectMolecule*)origObj,fname,frame,
                                  1,1,1,-1,-1,NULL,1,NULL,quiet);
        /* if(finish)
           ExecutiveUpdateObjectSelection(TempPyMOLGlobals,origObj); unnecc */
        sprintf(buf," CmdLoad: \"%s\" appended into object \"%s\".\n CmdLoad: %d total states in the object.\n",
                fname,oname,((ObjectMolecule*)origObj)->NCSet);
      } else {
        PRINTFB(TempPyMOLGlobals,FB_CCmd,FB_Errors)
          "CmdLoad-Error: must load object topology before loading trajectory!"
          ENDFB(TempPyMOLGlobals);
      }
      break;
    case cLoadTypeCRD:
      PRINTFD(TempPyMOLGlobals,FB_CCmd) " CmdLoad-DEBUG: loading CRD\n" ENDFD;
      if(origObj) { /* always reinitialize topology objects from scratch */
        ObjectMoleculeLoadRSTFile(TempPyMOLGlobals,(ObjectMolecule*)origObj,fname,frame,quiet);
        /* if(finish)
           ExecutiveUpdateObjectSelection(TempPyMOLGlobals,origObj); unnecc */
        sprintf(buf," CmdLoad: \"%s\" appended into object \"%s\".\n CmdLoad: %d total states in the object.\n",
                fname,oname,((ObjectMolecule*)origObj)->NCSet);
      } else {
        PRINTFB(TempPyMOLGlobals,FB_CCmd,FB_Errors)
          "CmdLoad-Error: must load object topology before loading coordinate file!"
          ENDFB(TempPyMOLGlobals);
      }
      break;
    case cLoadTypeRST:
      PRINTFD(TempPyMOLGlobals,FB_CCmd) " CmdLoad-DEBUG: loading RST\n" ENDFD;
      if(origObj) { /* always reinitialize topology objects from scratch */
        ObjectMoleculeLoadRSTFile(TempPyMOLGlobals,(ObjectMolecule*)origObj,fname,frame,quiet);
        /* if(finish)
           ExecutiveUpdateObjectSelection(TempPyMOLGlobals,origObj); unnecc */
        sprintf(buf," CmdLoad: \"%s\" appended into object \"%s\".\n CmdLoad: %d total states in the object.\n",
                fname,oname,((ObjectMolecule*)origObj)->NCSet);
      } else {
        PRINTFB(TempPyMOLGlobals,FB_CCmd,FB_Errors)
          "CmdLoad-Error: must load object topology before loading restart file!"
          ENDFB(TempPyMOLGlobals);
      }
      break;
    case cLoadTypePMO:
      PRINTFD(TempPyMOLGlobals,FB_CCmd) " CmdLoad-DEBUG: loading PMO\n" ENDFD;
      if(!origObj) {
        obj=(CObject*)ObjectMoleculeLoadPMOFile(TempPyMOLGlobals,NULL,fname,frame,discrete);
        if(obj) {
          ObjectSetName(obj,oname);
          ExecutiveManageObject(TempPyMOLGlobals,obj,zoom,true);
          if(frame<0)
            frame = ((ObjectMolecule*)obj)->NCSet-1;
          sprintf(buf," CmdLoad: \"%s\" loaded as \"%s\".\n",
                  fname,oname);
        }
      } else {
        ObjectMoleculeLoadPMOFile(TempPyMOLGlobals,(ObjectMolecule*)origObj,fname,frame,discrete);
        if(finish)
          ExecutiveUpdateObjectSelection(TempPyMOLGlobals,origObj);
        if(frame<0)
          frame = ((ObjectMolecule*)origObj)->NCSet-1;
        sprintf(buf," CmdLoad: \"%s\" appended into object \"%s\", state %d.\n",
                fname,oname,frame+1);
      }
      break;
    case cLoadTypeXYZ:
      PRINTFD(TempPyMOLGlobals,FB_CCmd) " CmdLoad-DEBUG: loading XYZStr\n" ENDFD;
      if(!origObj) {
        obj=(CObject*)ObjectMoleculeLoadXYZFile(TempPyMOLGlobals,NULL,fname,frame,discrete);
        if(obj) {
          ObjectSetName(obj,oname);
          ExecutiveManageObject(TempPyMOLGlobals,obj,zoom,true);
          if(frame<0)
            frame = ((ObjectMolecule*)obj)->NCSet-1;
          sprintf(buf," CmdLoad: \"%s\" loaded as \"%s\".\n",
                  fname,oname);
        }
      } else {
        ObjectMoleculeLoadXYZFile(TempPyMOLGlobals,(ObjectMolecule*)origObj,fname,frame,discrete);
        if(finish)
          ExecutiveUpdateObjectSelection(TempPyMOLGlobals,origObj);
        if(frame<0)
          frame = ((ObjectMolecule*)origObj)->NCSet-1;
        sprintf(buf," CmdLoad: \"%s\" appended into object \"%s\", state %d.\n",
                fname,oname,frame+1);
      }
      break;
    case cLoadTypePDBStr:
      ok = ExecutiveProcessPDBFile(TempPyMOLGlobals,origObj,fname,oname,
                                   frame,discrete,finish,buf,NULL,
                                   quiet,true,multiplex,zoom);
      break;
#if 0

    case cLoadTypeMOL:
      PRINTFD(TempPyMOLGlobals,FB_CCmd) " CmdLoad-DEBUG: loading MOL\n" ENDFD;
      obj=(CObject*)ObjectMoleculeLoadMOLFile(TempPyMOLGlobals,(ObjectMolecule*)origObj,fname,frame,discrete);
      if(!origObj) {
        if(obj) {
          ObjectSetName(obj,oname);
          ExecutiveManageObject(TempPyMOLGlobals,obj,zoom,true);
          if(frame<0)
            frame = ((ObjectMolecule*)obj)->NCSet-1;
          sprintf(buf," CmdLoad: \"%s\" loaded as \"%s\".\n",
                  fname,oname);		  
        }
      } else if(origObj) {
        if(finish)
          ExecutiveUpdateObjectSelection(TempPyMOLGlobals,origObj);
        if(frame<0)
          frame = ((ObjectMolecule*)origObj)->NCSet-1;
        sprintf(buf," CmdLoad: \"%s\" appended into object \"%s\", state %d.\n",
                fname,oname,frame+1);
      }
      break;
    case cLoadTypeMOLStr:
      PRINTFD(TempPyMOLGlobals,FB_CCmd) " CmdLoad-DEBUG: reading MOLStr\n" ENDFD;
      obj=(CObject*)ObjectMoleculeReadMOLStr(TempPyMOLGlobals,
                                             (ObjectMolecule*)origObj,fname,frame,discrete,finish);
      if(!origObj) {
        if(obj) {
          ObjectSetName(obj,oname);
          ExecutiveManageObject(TempPyMOLGlobals,obj,zoom,true);
          if(frame<0)
            frame = ((ObjectMolecule*)obj)->NCSet-1;
          sprintf(buf," CmdLoad: MOL-string loaded as \"%s\".\n",
                  oname);		  
        }
      } else if(origObj) {
        if(finish) {
          ObjectMoleculeInvalidate((ObjectMolecule*)origObj,cRepAll,cRepInvAll,-1);
          ObjectMoleculeUpdateIDNumbers((ObjectMolecule*)origObj);
          ObjectMoleculeUpdateNonbonded((ObjectMolecule*)origObj);
          ExecutiveUpdateObjectSelection(TempPyMOLGlobals,origObj);
        }
        if(frame<0)
          frame = ((ObjectMolecule*)origObj)->NCSet-1;
        sprintf(buf," CmdLoad: MOL-string appended into object \"%s\", state %d.\n",
                oname,frame+1);
      }
      break;
#else
    case cLoadTypeMOL:
    case cLoadTypeMOLStr:
#endif
    case cLoadTypeSDF2:
    case cLoadTypeSDF2Str:
    case cLoadTypeMOL2:
    case cLoadTypeMOL2Str:

      /*      ExecutiveLoadMOL2(TempPyMOLGlobals,origObj,fname,oname,frame,
              discrete,finish,buf,multiplex,quiet,false,zoom); */

      ok = ExecutiveLoad(TempPyMOLGlobals,origObj, 
                         fname, 0, type,
                         oname, frame, zoom, 
                         discrete, finish, 
                         multiplex, quiet);
      break;
      /*
    case cLoadTypeMOL2Str:
      ExecutiveLoadMOL2(TempPyMOLGlobals,origObj,fname,oname,frame,
                        discrete,finish,buf,multiplex,quiet,true,zoom);
                        break;*/

    case cLoadTypeMMD:
      PRINTFD(TempPyMOLGlobals,FB_CCmd) " CmdLoad-DEBUG: loading MMD\n" ENDFD;
      obj=(CObject*)ObjectMoleculeLoadMMDFile(TempPyMOLGlobals,(ObjectMolecule*)origObj,fname,frame,NULL,discrete);
      if(!origObj) {
        if(obj) {
          ObjectSetName(obj,oname);
          ExecutiveManageObject(TempPyMOLGlobals,obj,zoom,true);
          if(frame<0)
            frame = ((ObjectMolecule*)obj)->NCSet-1;
          sprintf(buf," CmdLoad: \"%s\" loaded as \"%s\".\n",
                  fname,oname);		  
        }
      } else if(origObj) {
        if(finish)
          ExecutiveUpdateObjectSelection(TempPyMOLGlobals,origObj);
        if(frame<0)
          frame = ((ObjectMolecule*)origObj)->NCSet-1;
        sprintf(buf," CmdLoad: \"%s\" appended into object \"%s\", state %d.\n",
                fname,oname,frame+1);
      }
      break;
    case cLoadTypeMMDSeparate:
      ObjectMoleculeLoadMMDFile(TempPyMOLGlobals,(ObjectMolecule*)origObj,fname,frame,oname,discrete);
      break;
    case cLoadTypeMMDStr:
      PRINTFD(TempPyMOLGlobals,FB_CCmd) " CmdLoad-DEBUG: loading MMDStr\n" ENDFD;
      obj=(CObject*)ObjectMoleculeReadMMDStr(TempPyMOLGlobals,(ObjectMolecule*)origObj,fname,frame,discrete);
      if(!origObj) {
        if(obj) {
          ObjectSetName(obj,oname);
          ExecutiveManageObject(TempPyMOLGlobals,obj,zoom,true);
          if(frame<0)
            frame = ((ObjectMolecule*)obj)->NCSet-1;
          sprintf(buf," CmdLoad: MMD-string loaded as \"%s\".\n",
                  oname);		  
        }
      } else if(origObj) {
        if(finish)
          ExecutiveUpdateObjectSelection(TempPyMOLGlobals,origObj);
        if(frame<0)
          frame = ((ObjectMolecule*)origObj)->NCSet-1;
        sprintf(buf," CmdLoad: MMD-string appended into object \"%s\", state %d\n",
                oname,frame+1);
      }
      break;
#if 0
    case cLoadTypeXPLORMap:
      PRINTFD(TempPyMOLGlobals,FB_CCmd) " CmdLoad-DEBUG: loading XPLORMap\n" ENDFD;
      if(!origObj) {
        obj=(CObject*)ObjectMapLoadXPLOR(TempPyMOLGlobals,NULL,fname,frame,true,quiet);
        if(obj) {
          ObjectSetName(obj,oname);
          ExecutiveManageObject(TempPyMOLGlobals,(CObject*)obj,zoom,true);
          sprintf(buf," CmdLoad: \"%s\" loaded as \"%s\".\n",fname,oname);
        }
      } else {
        ObjectMapLoadXPLOR(TempPyMOLGlobals,(ObjectMap*)origObj,fname,frame,true,quiet);
        sprintf(buf," CmdLoad: \"%s\" appended into object \"%s\".\n",
                fname,oname);
      }
      break;
    case cLoadTypeXPLORStr:
      PRINTFD(TempPyMOLGlobals,FB_CCmd) " CmdLoad-DEBUG: loading XPLOR string\n" ENDFD;
      if(!origObj) {
        obj=(CObject*)ObjectMapLoadXPLOR(TempPyMOLGlobals,NULL,fname,frame,false,quiet);
        if(obj) {
          ObjectSetName(obj,oname);
          ExecutiveManageObject(TempPyMOLGlobals,(CObject*)obj,zoom,true);
          sprintf(buf," CmdLoad: XPLOR string loaded as \"%s\".\n",oname);
        }
      } else {
        ObjectMapLoadXPLOR(TempPyMOLGlobals,(ObjectMap*)origObj,fname,frame,false,quiet);
        sprintf(buf," CmdLoad: XPLOR string appended into object \"%s\".\n",
                oname);
      }
      break;
    case cLoadTypeCCP4Map:
      PRINTFD(TempPyMOLGlobals,FB_CCmd) " CmdLoad-DEBUG: loading CCP4 map\n" ENDFD;
      if(!origObj) {
        obj=(CObject*)ObjectMapLoadCCP4(TempPyMOLGlobals,NULL,fname,frame,false,0,quiet);
        if(obj) {
          ObjectSetName(obj,oname);
          ExecutiveManageObject(TempPyMOLGlobals,(CObject*)obj,zoom,true);
          sprintf(buf," CmdLoad: \"%s\" loaded as \"%s\".\n",fname,oname);
        }
      } else {
        ObjectMapLoadCCP4(TempPyMOLGlobals,(ObjectMap*)origObj,fname,frame,false,0,quiet);
        sprintf(buf," CmdLoad: \"%s\" appended into object \"%s\".\n",
                fname,oname);
      }
      break;
    case cLoadTypeCCP4Str:
      PRINTFD(TempPyMOLGlobals,FB_CCmd) " CmdLoad-DEBUG: loading CCP4 map\n" ENDFD;
      if(!origObj) {
        obj=(CObject*)ObjectMapLoadCCP4(TempPyMOLGlobals,NULL,fname,frame,true,bytes,quiet);
        if(obj) {
          ObjectSetName(obj,oname);
          ExecutiveManageObject(TempPyMOLGlobals,(CObject*)obj,zoom,true);
          sprintf(buf," CmdLoad: \"%s\" loaded as \"%s\".\n",fname,oname);
        }
      } else {
        ObjectMapLoadCCP4(TempPyMOLGlobals,(ObjectMap*)origObj,fname,frame,true,bytes,quiet);
        sprintf(buf," CmdLoad: \"%s\" appended into object \"%s\".\n",
                fname,oname);
      }
      break;
#else
    case cLoadTypeXPLORMap:     
    case cLoadTypeXPLORStr:
    case cLoadTypeCCP4Map:
    case cLoadTypeCCP4Str:
      ok = ExecutiveLoad(TempPyMOLGlobals,origObj, 
                         fname, bytes, type,
                         oname, frame, zoom, 
                         discrete, finish, 
                         multiplex, quiet);
      break;
#endif

    case cLoadTypePHIMap:
      PRINTFD(TempPyMOLGlobals,FB_CCmd) " CmdLoad-DEBUG: loading Delphi Map\n" ENDFD;
      if(!origObj) {
        obj=(CObject*)ObjectMapLoadPHIFile(TempPyMOLGlobals,NULL,fname,frame);
        if(obj) {
          ObjectSetName(obj,oname);
          ExecutiveManageObject(TempPyMOLGlobals,(CObject*)obj,zoom,true);
          sprintf(buf," CmdLoad: \"%s\" loaded as \"%s\".\n",fname,oname);
        }
      } else {
        ObjectMapLoadPHIFile(TempPyMOLGlobals,(ObjectMap*)origObj,fname,frame);
        sprintf(buf," CmdLoad: \"%s\" appended into object \"%s\".\n",
                fname,oname);
      }
      break;
    case cLoadTypeDXMap:
      PRINTFD(TempPyMOLGlobals,FB_CCmd) " CmdLoad-DEBUG: loading DX Map\n" ENDFD;
      if(!origObj) {
        obj=(CObject*)ObjectMapLoadDXFile(TempPyMOLGlobals,NULL,fname,frame);
        if(obj) {
          ObjectSetName(obj,oname);
          ExecutiveManageObject(TempPyMOLGlobals,(CObject*)obj,zoom,true);
          sprintf(buf," CmdLoad: \"%s\" loaded as \"%s\".\n",fname,oname);
        }
      } else {
        ObjectMapLoadDXFile(TempPyMOLGlobals,(ObjectMap*)origObj,fname,frame);
        sprintf(buf," CmdLoad: \"%s\" appended into object \"%s\".\n",
                fname,oname);
      }
      break;
    case cLoadTypeFLDMap:
      PRINTFD(TempPyMOLGlobals,FB_CCmd) " CmdLoad-DEBUG: loading AVS Map\n" ENDFD;
      if(!origObj) {
        obj=(CObject*)ObjectMapLoadFLDFile(TempPyMOLGlobals,NULL,fname,frame);
        if(obj) {
          ObjectSetName(obj,oname);
          ExecutiveManageObject(TempPyMOLGlobals,(CObject*)obj,zoom,true);
          sprintf(buf," CmdLoad: \"%s\" loaded as \"%s\".\n",fname,oname);
        }
      } else {
        ObjectMapLoadFLDFile(TempPyMOLGlobals,(ObjectMap*)origObj,fname,frame);
        sprintf(buf," CmdLoad: \"%s\" appended into object \"%s\".\n",
                fname,oname);
      }
      break;
    case cLoadTypeBRIXMap:
      PRINTFD(TempPyMOLGlobals,FB_CCmd) " CmdLoad-DEBUG: loading BRIX/DSN6 Map\n" ENDFD;
      if(!origObj) {
        obj=(CObject*)ObjectMapLoadBRIXFile(TempPyMOLGlobals,NULL,fname,frame);
        if(obj) {
          ObjectSetName(obj,oname);
          ExecutiveManageObject(TempPyMOLGlobals,(CObject*)obj,zoom,true);
          sprintf(buf," CmdLoad: \"%s\" loaded as \"%s\".\n",fname,oname);
        }
      } else {
        ObjectMapLoadFLDFile(TempPyMOLGlobals,(ObjectMap*)origObj,fname,frame);
        sprintf(buf," CmdLoad: \"%s\" appended into object \"%s\".\n",
                fname,oname);
      }
      break;
    case cLoadTypeGRDMap:
      PRINTFD(TempPyMOLGlobals,FB_CCmd) " CmdLoad-DEBUG: loading GRD Map\n" ENDFD;
      if(!origObj) {
        obj=(CObject*)ObjectMapLoadGRDFile(TempPyMOLGlobals,NULL,fname,frame);
        if(obj) {
          ObjectSetName(obj,oname);
          ExecutiveManageObject(TempPyMOLGlobals,(CObject*)obj,zoom,true);
          sprintf(buf," CmdLoad: \"%s\" loaded as \"%s\".\n",fname,oname);
        }
      } else {
        ObjectMapLoadGRDFile(TempPyMOLGlobals,(ObjectMap*)origObj,fname,frame);
        sprintf(buf," CmdLoad: \"%s\" appended into object \"%s\".\n",
                fname,oname);
      }
      break;
    }
    if(!quiet && buf[0]) {
      PRINTFB(TempPyMOLGlobals,FB_Executive,FB_Actions) 
        "%s",buf
        ENDFB(TempPyMOLGlobals);
    }
    OrthoRestorePrompt(TempPyMOLGlobals);
    APIExit();
  }
  return APIResultOk(ok);
}


static PyObject *CmdLoadTraj(PyObject *self, PyObject *args)
{
  char *fname,*oname;
  CObject *origObj = NULL;
  OrthoLineType buf;
  int frame,type;
  int interval,average,start,stop,max,image;
  OrthoLineType s1;
  char *str1;
  int ok=false;
  float shift[3];
  int quiet=0; /* TODO */
  char *plugin = NULL;
  ok = PyArg_ParseTuple(args,"ssiiiiiiisifffs",&oname,&fname,&frame,&type,
                        &interval,&average,&start,&stop,&max,&str1,
                        &image,&shift[0],&shift[1],&shift[2],&plugin);

  buf[0]=0;
  if (ok) {
    APIEntry();
    if(str1[0])
      ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    else
      s1[0]=0; /* no selection */
    origObj=ExecutiveFindObjectByName(TempPyMOLGlobals,oname);
    /* check for existing object of right type, delete if not */
    if(origObj) {
      if (origObj->type != cObjectMolecule) {
        ExecutiveDelete(TempPyMOLGlobals,origObj->Name);
        origObj=NULL;
      }
    }
    if((type==cLoadTypeTRJ)&&(plugin[0])) 
      type = cLoadTypeTRJ2;
    /*printf("plugin %s %d\n",plugin,type);*/
    if(origObj) {
      switch(type) {
      case cLoadTypeXTC:
      case cLoadTypeTRR:
      case cLoadTypeGRO:
      case cLoadTypeG96:
      case cLoadTypeTRJ2:
      case cLoadTypeDCD:
        PlugIOManagerLoadTraj(TempPyMOLGlobals,(ObjectMolecule*)origObj,fname,frame,
                              interval,average,start,stop,max,s1,image,shift,quiet,plugin);
        break;
      case cLoadTypeTRJ: /* this is the ascii AMBER trajectory format... */
        PRINTFD(TempPyMOLGlobals,FB_CCmd) " CmdLoadTraj-DEBUG: loading TRJ\n" ENDFD;
        ObjectMoleculeLoadTRJFile(TempPyMOLGlobals,(ObjectMolecule*)origObj,fname,frame,
                                  interval,average,start,stop,max,s1,image,shift,quiet);
        /* if(finish)
           ExecutiveUpdateObjectSelection(TempPyMOLGlobals,origObj); unnecc */
        sprintf(buf," CmdLoadTraj: \"%s\" appended into object \"%s\".\n CmdLoadTraj: %d total states in the object.\n",
                fname,oname,((ObjectMolecule*)origObj)->NCSet);
        break;
      }
    } else {
      PRINTFB(TempPyMOLGlobals,FB_CCmd,FB_Errors)
        "CmdLoadTraj-Error: must load object topology before loading trajectory.\n"
        ENDFB(TempPyMOLGlobals);
    }
    if(origObj) {
      PRINTFB(TempPyMOLGlobals,FB_Executive,FB_Actions) 
        "%s",buf
        ENDFB(TempPyMOLGlobals);
      OrthoRestorePrompt(TempPyMOLGlobals);
    }
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdOrigin(PyObject *self, PyObject *args)
{
  char *str1,*obj;
  OrthoLineType s1;
  float v[3];
  int ok=false;
  int state;
  ok = PyArg_ParseTuple(args,"ss(fff)i",&str1,&obj,v,v+1,v+2,&state);
  if (ok) {
    APIEntry();
    if(str1[0])
      ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    else
      s1[0]=0; /* no selection */
    ok = ExecutiveOrigin(TempPyMOLGlobals,s1,1,obj,v,state); /* TODO STATUS */
    if(str1[0])
      SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdSort(PyObject *self, PyObject *args)
{
  char *name;
  int ok=false;
  ok = PyArg_ParseTuple(args,"s",&name);
  if (ok) {
    APIEntry();
    ExecutiveSort(TempPyMOLGlobals,name); /* TODO STATUS */
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdAssignSS(PyObject *self, PyObject *args)
/* EXPERIMENTAL */
{
  int ok=false;
  int state,quiet;
  char *str1,*str2;
  int preserve;
  OrthoLineType s1,s2;
  ok = PyArg_ParseTuple(args,"sisii",&str1,&state,&str2,&preserve,&quiet);
  if (ok) {
    APIEntry();
    ok = ((SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0) &&
          (SelectorGetTmp(TempPyMOLGlobals,str2,s2)>=0));
    if(ok) ok = ExecutiveAssignSS(TempPyMOLGlobals,s1,state,s2,preserve,quiet);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdSpheroid(PyObject *self, PyObject *args)
/* EXPERIMENTAL */
{
  char *name;
  int ok=false;
  int average;
  ok = PyArg_ParseTuple(args,"si",&name,&average);
  if (ok) {
    APIEntry();
    ExecutiveSpheroid(TempPyMOLGlobals,name,average); /* TODO STATUS */
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdTest(PyObject *self, PyObject *args)
{
  /* regression tests */

  int ok=true;
  int code;
  int group;

  ok = PyArg_ParseTuple(args,"ii",&group,&code);
  if(ok) {
    APIEntry();
    PRINTFB(TempPyMOLGlobals,FB_CCmd,FB_Details)
      " Cmd: initiating test %d-%d.\n",group,code
      ENDFB(TempPyMOLGlobals);
    ok = TestPyMOLRun(TempPyMOLGlobals,group,code);
    PRINTFB(TempPyMOLGlobals,FB_CCmd,FB_Details)
      " Cmd: concluding test %d-%d.\n",group,code
      ENDFB(TempPyMOLGlobals);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdCenter(PyObject *self, PyObject *args)
{
  char *str1;
  OrthoLineType s1;
  int state;
  int origin;
  int ok=false;
  float animate;
  int quiet=false; /* TODO */
  ok = PyArg_ParseTuple(args,"siif",&str1,&state,&origin,&animate);
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    if (ok) ok = ExecutiveCenter(TempPyMOLGlobals,s1,state,origin,animate,NULL,quiet);
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdZoom(PyObject *self, PyObject *args)
{
  char *str1;
  OrthoLineType s1;
  float buffer;
  int state;
  int inclusive;
  int ok=false;
  float animate;
  int quiet=false; /* TODO */
  ok = PyArg_ParseTuple(args,"sfiif",&str1,&buffer,&state,&inclusive,&animate);
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    if(ok) ok = ExecutiveWindowZoom(TempPyMOLGlobals,s1,buffer,state,
                                    inclusive,animate,quiet); 
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdIsolevel(PyObject *self, PyObject *args)
{
  float level;
  int state;
  char *name;
  int ok=false;
  ok = PyArg_ParseTuple(args,"sfi",&name,&level,&state);
  if (ok) {
    APIEntry();
    ok = ExecutiveIsolevel(TempPyMOLGlobals,name,level,state);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdHAdd(PyObject *self, PyObject *args)
{
  char *str1;
  OrthoLineType s1;
  int quiet;
  int ok=false;
  ok = PyArg_ParseTuple(args,"si",&str1,&quiet);
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    ExecutiveAddHydrogens(TempPyMOLGlobals,s1,quiet); /* TODO STATUS */
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    ExecutiveAddHydrogens(TempPyMOLGlobals,s1,quiet); /* TODO STATUS */
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    ExecutiveAddHydrogens(TempPyMOLGlobals,s1,quiet); /* TODO STATUS */
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    ExecutiveAddHydrogens(TempPyMOLGlobals,s1,quiet); /* TODO STATUS */
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdGetObjectColorIndex(PyObject *self, PyObject *args)
{
  char *str1;
  int result = -1;
  int ok=false;
  ok = PyArg_ParseTuple(args,"s",&str1);
  if (ok) {
    APIEntry();
    result = ExecutiveGetObjectColorIndex(TempPyMOLGlobals,str1);
    APIExit();
  }
  return(APIResultCode(result));
}

static PyObject *CmdRemove(PyObject *self, PyObject *args)
{
  char *str1;
  OrthoLineType s1;
  int quiet;
  int ok=false;
  ok = PyArg_ParseTuple(args,"si",&str1,&quiet);
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    ExecutiveRemoveAtoms(TempPyMOLGlobals,s1,quiet); /* TODO STATUS */
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdRemovePicked(PyObject *self, PyObject *args)
{
  int i1;
  int ok=false;
  int quiet;
  ok = PyArg_ParseTuple(args,"ii",&i1,&quiet);
  if (ok) {
    APIEntry();
    EditorRemove(TempPyMOLGlobals,i1,quiet); /* TODO STATUS */
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdHFill(PyObject *self, PyObject *args)
{
  int ok = true;
  int quiet;
  ok = PyArg_ParseTuple(args,"i",&quiet);
  if(ok) {
    APIEntry();
    EditorHFill(TempPyMOLGlobals,quiet); /* TODO STATUS */
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdHFix(PyObject *self, PyObject *args)
{
  int ok = true;
  int quiet;
  OrthoLineType s1;
  char *str1;
  ok = PyArg_ParseTuple(args,"si",&str1,&quiet);
  if(ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    EditorHFix(TempPyMOLGlobals,s1,quiet); /* TODO STATUS */
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdCycleValence(PyObject *self, PyObject *args)
{
  int ok = true;
  int quiet;
  ok = PyArg_ParseTuple(args,"i",&quiet);
  if(ok) {
    APIEntry();
    EditorCycleValence(TempPyMOLGlobals,quiet);  /* TODO STATUS */
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdReplace(PyObject *self, 	PyObject *args)
{
  int i1,i2;
  char *str1,*str2;
  int ok=false;
  int quiet;
  ok = PyArg_ParseTuple(args,"siisi",&str1,&i1,&i2,&str2,&quiet);
  if (ok) {
    APIEntry();
    EditorReplace(TempPyMOLGlobals,str1,i1,i2,str2,quiet);  /* TODO STATUS */
    APIExit();
  }
  return APIResultOk(ok);  
}

static PyObject *CmdSetGeometry(PyObject *self, 	PyObject *args)
{
  int i1,i2;
  char *str1;
  OrthoLineType s1;
  int ok=false;
  ok = PyArg_ParseTuple(args,"sii",&str1,&i1,&i2);
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    ok = ExecutiveSetGeometry(TempPyMOLGlobals,s1,i1,i2);  /* TODO STATUS */
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);  
}

static PyObject *CmdAttach(PyObject *self, 	PyObject *args)
{
  int i1,i2;
  char *str1;
  int ok=false;
  int quiet;
  char *name;
  ok = PyArg_ParseTuple(args,"siis",&str1,&i1,&i2,&name,&quiet);
  if (ok) {
    APIEntry();
    EditorAttach(TempPyMOLGlobals,str1,i1,i2,name,quiet);  /* TODO STATUS */
    APIExit();
  }
  return APIResultOk(ok);  
}

static PyObject *CmdFuse(PyObject *self, 	PyObject *args)
{
  char *str1,*str2;
  int mode;
  OrthoLineType s1,s2;
  int recolor;
  int ok=false;
  int move_flag;
  ok = PyArg_ParseTuple(args,"ssiii",&str1,&str2,&mode,&recolor,&move_flag);
  if (ok) {
    APIEntry();
    ok = ((SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0) &&
          (SelectorGetTmp(TempPyMOLGlobals,str2,s2)>=0));
    ExecutiveFuse(TempPyMOLGlobals,s1,s2,mode,recolor,move_flag);  /* TODO STATUS */
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    SelectorFreeTmp(TempPyMOLGlobals,s2);
    APIExit();
  }
  return APIResultOk(ok);  
}

static PyObject *CmdUnpick(PyObject *self, 	PyObject *args)
{
  APIEntry();
  EditorInactivate(TempPyMOLGlobals);  /* TODO STATUS */
  APIExit();
  return APISuccess();  
}

static PyObject *CmdEdit(PyObject *self, 	PyObject *args)
{
  char *str0,*str1,*str2,*str3;
  OrthoLineType s0 = "";
  OrthoLineType s1 = "";
  OrthoLineType s2 = "";
  OrthoLineType s3 = "";
  int pkresi,pkbond;
  int ok=false;
  int quiet;
  ok = PyArg_ParseTuple(args,"ssssiii",&str0,&str1,&str2,&str3,&pkresi,&pkbond,&quiet);
  if (ok) {
    APIEntry();
    if(!str0[0]) {
      EditorInactivate(TempPyMOLGlobals);
    } else {
      ok = (SelectorGetTmp(TempPyMOLGlobals,str0,s0)>=0);
      if(str1[0]) ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
      if(str2[0]) ok = (SelectorGetTmp(TempPyMOLGlobals,str2,s2)>=0);
      if(str3[0]) ok = (SelectorGetTmp(TempPyMOLGlobals,str3,s3)>=0);
      ok = EditorSelect(TempPyMOLGlobals,s0,s1,s2,s3,pkresi,pkbond,quiet);
      if(s0[0]) SelectorFreeTmp(TempPyMOLGlobals,s0);
      if(s1[0]) SelectorFreeTmp(TempPyMOLGlobals,s1);
      if(s2[0]) SelectorFreeTmp(TempPyMOLGlobals,s2);
      if(s3[0]) SelectorFreeTmp(TempPyMOLGlobals,s3);
    }
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdDrag(PyObject *self, 	PyObject *args)
{
  char *str0;
  OrthoLineType s0 = "";
  int ok=true;
  int quiet;
  ok = PyArg_ParseTuple(args,"si",&str0,&quiet);
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str0,s0)>=0);
    if(ok) {
      ok = ExecutiveSetDrag(TempPyMOLGlobals,s0,quiet);
      SelectorFreeTmp(TempPyMOLGlobals,s0);
    }
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdRename(PyObject *self, 	PyObject *args)
{
  char *str1;
  int int1;
  OrthoLineType s1;

  int ok=false;
  ok = PyArg_ParseTuple(args,"si",&str1,&int1);
  if (ok) {
    APIEntry();
    ok = (SelectorGetTmp(TempPyMOLGlobals,str1,s1)>=0);
    ExecutiveRenameObjectAtoms(TempPyMOLGlobals,s1,int1); /* TODO STATUS */
    SelectorFreeTmp(TempPyMOLGlobals,s1);
    APIExit();
  }
  return APIResultOk(ok);  
}

static PyObject *CmdOrder(PyObject *self, 	PyObject *args)
{
  char *str1;
  int int1,int2;

  int ok=false;
  ok = PyArg_ParseTuple(args,"sii",&str1,&int1,&int2);
  if (ok) {
    APIEntry();
    ok = ExecutiveOrder(TempPyMOLGlobals,str1,int1,int2);
    APIExit();
  }
  return APIResultOk(ok);
}

static PyObject *CmdWindow(PyObject *self, 	PyObject *args)
{
  int int1,x,y,width,height;
  int ok=true;
  ok = PyArg_ParseTuple(args,"iiiii",&int1,&x,&y,&width,&height);
  if (ok) {
    APIEntry();
    switch(int1) {
#ifndef _PYMOL_NO_MAIN
    case 0:
    case 1:
      MainSetWindowVisibility(int1);
      break;
    case 2: /* position */
      MainSetWindowPosition(TempPyMOLGlobals,x,y);
      break;
    case 3: /* size */
      if((width==0)&&(height==0)&&(x!=0)&&(y!=0)) {
        width=x;
        height=y;
      }
      MainSetWindowSize(TempPyMOLGlobals,width,height);
      break;
    case 4: /* position and size */
      MainSetWindowPosition(TempPyMOLGlobals,x,y);
      MainSetWindowSize(TempPyMOLGlobals,width,height);
      break;
    case 5: /* maximize -- 
               should use the window manager, 
               but GLUT doesn't provide for that */
      MainMaximizeWindow(TempPyMOLGlobals);
      break;
    case 6:
      MainCheckWindowFit(TempPyMOLGlobals);
      break;
#endif 
#ifdef _MACPYMOL_XCODE
    default:
	   do_window(int1,x,y,width,height);
	   break;
#endif	  
	}
   
    APIExit();
  }
  return APIResultOk(ok);  
}


static PyObject *CmdGetCThreadingAPI(PyObject *self, 	PyObject *args)
{
  PyObject *result = PyList_New(2);
  PyList_SetItem(result,0,PyCObject_FromVoidPtr((void*)PBlock,NULL));
  PyList_SetItem(result,1,PyCObject_FromVoidPtr((void*)PUnblock,NULL));
  return result;
}

static PyMethodDef Cmd_methods[] = {
   {"_get_c_threading_api",  CmdGetCThreadingAPI,         METH_VARARGS },
   {"_set_scene_names",    CmdSetSceneNames,           METH_VARARGS },
	{"accept",	              CmdAccept,               METH_VARARGS },
	{"align",	              CmdAlign,                METH_VARARGS },
	{"alter",	              CmdAlter,                METH_VARARGS },
	{"alter_list",            CmdAlterList,            METH_VARARGS },
	{"alter_state",           CmdAlterState,           METH_VARARGS },
   {"angle",                 CmdAngle,                METH_VARARGS },
	{"attach",                CmdAttach,               METH_VARARGS },
   {"bg_color",              CmdBackgroundColor,      METH_VARARGS },
	{"bond",                  CmdBond,                 METH_VARARGS },
   {"busy_draw",             CmdBusyDraw,             METH_VARARGS },
   {"button",                CmdButton,               METH_VARARGS },
   {"cartoon",               CmdCartoon,              METH_VARARGS },
   {"center",                CmdCenter,               METH_VARARGS },
	{"clip",	                 CmdClip,                 METH_VARARGS },
	{"cls",	                 CmdCls,                  METH_VARARGS },
	{"color",	              CmdColor,                METH_VARARGS },
	{"colordef",	           CmdColorDef,             METH_VARARGS },
   {"combine_object_ttt",    CmdCombineObjectTTT,     METH_VARARGS },
    {"coordset_update_thread",  CmdCoordSetUpdateThread,       METH_VARARGS },
	{"copy",                  CmdCopy,                 METH_VARARGS },
	{"create",                CmdCreate,               METH_VARARGS },
	{"count_states",          CmdCountStates,          METH_VARARGS },
	{"count_frames",          CmdCountFrames,          METH_VARARGS },
	{"cycle_valence",         CmdCycleValence,         METH_VARARGS },
   {"debug",                 CmdDebug,                METH_VARARGS },
   {"decline",               CmdDecline,              METH_VARARGS },
   {"del_colorection",       CmdDelColorection,       METH_VARARGS },   
   {"fake_drag",             CmdFakeDrag,             METH_VARARGS },   
   {"gl_delete_lists",       CmdGLDeleteLists,        METH_VARARGS },
	{"delete",                CmdDelete,               METH_VARARGS },
	{"dirty",                 CmdDirty,                METH_VARARGS },
	{"dirty_wizard",          CmdDirtyWizard,          METH_VARARGS },
   {"dihedral",              CmdDihedral,             METH_VARARGS },
   /*	{"distance",	        CmdDistance,             METH_VARARGS }, * abandoned long ago, right? */

	{"dist",    	           CmdDist,                 METH_VARARGS },
	{"do",	                 CmdDo,                   METH_VARARGS },
   {"draw",                  CmdDraw,                 METH_VARARGS },
   {"drag",                  CmdDrag,                 METH_VARARGS },
	{"dump",	                 CmdDump,                 METH_VARARGS },
   {"edit",                  CmdEdit,                 METH_VARARGS },
   {"torsion",               CmdTorsion,              METH_VARARGS },
	{"export_dots",           CmdExportDots,           METH_VARARGS },
	{"export_coords",         CmdExportCoords,         METH_VARARGS },
	{"feedback",              CmdFeedback,             METH_VARARGS },
	{"find_pairs",            CmdFindPairs,            METH_VARARGS },
	{"finish_object",         CmdFinishObject,         METH_VARARGS },
	{"fit",                   CmdFit,                  METH_VARARGS },
	{"fit_pairs",             CmdFitPairs,             METH_VARARGS },
	{"fix_chemistry",         CmdFixChemistry,         METH_VARARGS },
	{"flag",                  CmdFlag,                 METH_VARARGS },
	{"frame",	              CmdFrame,                METH_VARARGS },
   {"flush_now",             CmdFlushNow,             METH_VARARGS },
   {"delete_colorection",    CmdDelColorection,       METH_VARARGS },   
	{"dss",   	              CmdAssignSS,             METH_VARARGS },
   {"full_screen",           CmdFullScreen,           METH_VARARGS },
   {"fuse",                  CmdFuse,                 METH_VARARGS },
	{"get",	                 CmdGet,                  METH_VARARGS },
	{"get_angle",             CmdGetAngle,             METH_VARARGS },
	{"get_area",              CmdGetArea,              METH_VARARGS },
   {"get_atom_coords",       CmdGetAtomCoords,        METH_VARARGS },
   {"get_bond_print",        CmdGetBondPrint,         METH_VARARGS },
   {"get_busy",              CmdGetBusy,              METH_VARARGS },
	{"get_chains",            CmdGetChains,            METH_VARARGS },
	{"get_color",             CmdGetColor,             METH_VARARGS },
   {"get_colorection",       CmdGetColorection,       METH_VARARGS },   
	{"get_distance",          CmdGetDistance,          METH_VARARGS },
	{"get_dihe",              CmdGetDihe,              METH_VARARGS },
   {"get_editor_scheme",      CmdGetEditorScheme,      METH_VARARGS },
	{"get_frame",             CmdGetFrame,             METH_VARARGS },
	{"get_feedback",          CmdGetFeedback,          METH_VARARGS },
	{"get_matrix",	           CmdGetMatrix,            METH_VARARGS },
	{"get_min_max",           CmdGetMinMax,            METH_VARARGS },
   {"get_mtl_obj",            CmdGetMtlObj,            METH_VARARGS },
	{"get_model",	           CmdGetModel,             METH_VARARGS },
	{"get_moment",	           CmdGetMoment,            METH_VARARGS },
   {"get_movie_locked",      CmdGetMovieLocked,       METH_VARARGS },
   {"get_movie_playing",     CmdGetMoviePlaying,      METH_VARARGS },
   {"get_names",             CmdGetNames,             METH_VARARGS },
   {"get_object_color_index",CmdGetObjectColorIndex,  METH_VARARGS },
   {"get_object_matrix"     ,CmdGetObjectMatrix,      METH_VARARGS },
   {"get_origin"            , CmdGetOrigin,           METH_VARARGS },
	{"get_position",	        CmdGetPosition,          METH_VARARGS },
	{"get_povray",	           CmdGetPovRay,            METH_VARARGS },
   {"get_progress",          CmdGetProgress,          METH_VARARGS },
	{"get_pdb",	              CmdGetPDB,               METH_VARARGS },
   {"get_phipsi",            CmdGetPhiPsi,            METH_VARARGS },
   {"get_renderer",          CmdGetRenderer,          METH_VARARGS },
   {"get_raw_alignment",      CmdGetRawAlignment,         METH_VARARGS },
   {"get_seq_align_str",     CmdGetSeqAlignStr,          METH_VARARGS },
   {"get_session",           CmdGetSession,           METH_VARARGS },
	{"get_setting",           CmdGetSetting,           METH_VARARGS },
	{"get_setting_of_type",   CmdGetSettingOfType,    METH_VARARGS },
	{"get_setting_tuple",     CmdGetSettingTuple,      METH_VARARGS },
	{"get_setting_text",      CmdGetSettingText,       METH_VARARGS },
   {"get_setting_updates",   CmdGetSettingUpdates,    METH_VARARGS },
   {"get_object_list",       CmdGetObjectList,        METH_VARARGS },
   {"get_symmetry",          CmdGetCrystal,           METH_VARARGS },
	{"get_state",             CmdGetState,             METH_VARARGS },
   {"get_title",             CmdGetTitle,             METH_VARARGS },
	{"get_type",              CmdGetType,              METH_VARARGS },
   {"get_version",           CmdGetVersion,           METH_VARARGS },
   {"get_view",              CmdGetView,              METH_VARARGS },
   {"get_vis",               CmdGetVis,               METH_VARARGS },
   {"get_vrml",	             CmdGetVRML,            METH_VARARGS },
   {"get_wizard",            CmdGetWizard,            METH_VARARGS },
   {"get_wizard_stack",      CmdGetWizardStack,       METH_VARARGS },
	{"h_add",                 CmdHAdd,                 METH_VARARGS },
	{"h_fill",                CmdHFill,                METH_VARARGS },
	{"h_fix",                CmdHFix,                METH_VARARGS },
   {"identify",              CmdIdentify,             METH_VARARGS },
	{"import_coords",         CmdImportCoords,         METH_VARARGS },
   {"index",                 CmdIndex,                METH_VARARGS },
	{"intrafit",              CmdIntraFit,             METH_VARARGS },
   {"invert",                CmdInvert,               METH_VARARGS },
   {"interrupt",             CmdInterrupt,            METH_VARARGS },
	{"isolevel",              CmdIsolevel,             METH_VARARGS },
	{"isomesh",	              CmdIsomesh,              METH_VARARGS }, 
	{"isosurface",	           CmdIsosurface,           METH_VARARGS },
   {"wait_deferred",         CmdWaitDeferred,         METH_VARARGS },
   {"wait_queue",            CmdWaitQueue,            METH_VARARGS },
   {"label",                 CmdLabel,                METH_VARARGS },
	{"load",	                 CmdLoad,                 METH_VARARGS },
	{"load_color_table",	     CmdLoadColorTable,       METH_VARARGS },
	{"load_coords",           CmdLoadCoords,           METH_VARARGS },
	{"load_png",              CmdLoadPNG,              METH_VARARGS },
	{"load_object",           CmdLoadObject,           METH_VARARGS },
	{"load_traj",             CmdLoadTraj,             METH_VARARGS },
   {"map_new",               CmdMapNew,               METH_VARARGS },
   {"map_double",            CmdMapDouble,            METH_VARARGS },
   {"map_halve",            CmdMapHalve,            METH_VARARGS },
   {"map_set_border",        CmdMapSetBorder,         METH_VARARGS },
   {"map_trim",                  CmdMapTrim,         METH_VARARGS },
	{"mask",	                 CmdMask,                 METH_VARARGS },
	{"mclear",	              CmdMClear,               METH_VARARGS },
	{"mdo",	                 CmdMDo,                  METH_VARARGS },
	{"mdump",	              CmdMDump,                METH_VARARGS },
	{"mem",	                 CmdMem,                  METH_VARARGS },
	{"move",	                 CmdMove,                 METH_VARARGS },
	{"mset",	                 CmdMSet,                 METH_VARARGS },
	{"mplay",	              CmdMPlay,                METH_VARARGS },
	{"mpng_",	              CmdMPNG,                 METH_VARARGS },
	{"mmatrix",	              CmdMMatrix,              METH_VARARGS },
	{"multisave",             CmdMultiSave,            METH_VARARGS },
	{"mview",	              CmdMView,                METH_VARARGS },
    {"object_update_thread",  CmdObjectUpdateThread,       METH_VARARGS },
	{"origin",	              CmdOrigin,               METH_VARARGS },
	{"orient",	              CmdOrient,               METH_VARARGS },
	{"onoff",                 CmdOnOff,                METH_VARARGS },
   {"onoff_by_sele",         CmdOnOffBySele,          METH_VARARGS },
   {"order",                 CmdOrder,                METH_VARARGS },
	{"overlap",               CmdOverlap,              METH_VARARGS },
	{"p_glut_event",          CmdPGlutEvent,           METH_VARARGS },
   {"p_glut_get_redisplay",  CmdPGlutGetRedisplay,    METH_VARARGS },
	{"paste",	              CmdPaste,                METH_VARARGS },
	{"png",	                 CmdPNG,                  METH_VARARGS },
	{"pop",	                 CmdPop,                  METH_VARARGS },
	{"protect",	              CmdProtect,              METH_VARARGS },
	{"push_undo",	           CmdPushUndo,             METH_VARARGS },
	{"quit",	                 CmdQuit,                 METH_VARARGS },
   {"ray_trace_thread",      CmdRayTraceThread,       METH_VARARGS },
   {"ray_hash_thread",       CmdRayHashThread,        METH_VARARGS },
   {"ray_anti_thread",       CmdRayAntiThread,        METH_VARARGS },
   {"ramp_new",              CmdRampNew,           METH_VARARGS },
	{"ready",                 CmdReady,                METH_VARARGS },
   {"rebuild",               CmdRebuild,              METH_VARARGS },
   {"recolor",               CmdRecolor,              METH_VARARGS },
	{"refresh",               CmdRefresh,              METH_VARARGS },
	{"refresh_now",           CmdRefreshNow,           METH_VARARGS },
	{"refresh_wizard",        CmdRefreshWizard,        METH_VARARGS },
	{"remove",	              CmdRemove,               METH_VARARGS },
	{"remove_picked",         CmdRemovePicked,         METH_VARARGS },
	{"render",	              CmdRay,                  METH_VARARGS },
   {"rename",                CmdRename,               METH_VARARGS },
   {"replace",               CmdReplace,              METH_VARARGS },
   {"reinitialize",          CmdReinitialize,         METH_VARARGS },
	{"reset",                 CmdReset,                METH_VARARGS },
	{"reset_rate",	           CmdResetRate,            METH_VARARGS },
	{"reset_matrix",	        CmdResetMatrix,          METH_VARARGS },
   /*	{"rgbfunction",       CmdRGBFunction,              METH_VARARGS },*/
	{"rock",	                 CmdRock,                 METH_VARARGS },
	{"runpymol",	           CmdRunPyMOL,             METH_VARARGS },
	{"runwxpymol",	           CmdRunWXPyMOL,           METH_VARARGS },
	{"select",                CmdSelect,               METH_VARARGS },
	{"select_list",           CmdSelectList,           METH_VARARGS },
	{"set",	                 CmdSet,                  METH_VARARGS },
	{"legacy_set",            CmdLegacySet,            METH_VARARGS },

	{"sculpt_deactivate",     CmdSculptDeactivate,     METH_VARARGS },
	{"sculpt_activate",       CmdSculptActivate,       METH_VARARGS },
	{"sculpt_iterate",        CmdSculptIterate,        METH_VARARGS },
	{"sculpt_purge",          CmdSculptPurge,          METH_VARARGS },
   {"set_busy",              CmdSetBusy,              METH_VARARGS },   
   {"set_colorection",       CmdSetColorection,       METH_VARARGS },   
   {"set_colorection_name",  CmdSetColorectionName,   METH_VARARGS },   
	{"set_dihe",              CmdSetDihe,              METH_VARARGS },
	{"set_dihe",              CmdSetDihe,              METH_VARARGS },

	{"set_dihe",              CmdSetDihe,              METH_VARARGS },
	{"set_feedback",          CmdSetFeedbackMask,      METH_VARARGS },
	{"set_name",              CmdSetName,              METH_VARARGS },
   {"set_geometry",          CmdSetGeometry,          METH_VARARGS },
	{"set_matrix",	           CmdSetMatrix,            METH_VARARGS },
	{"set_object_ttt",        CmdSetObjectTTT,         METH_VARARGS },
   {"set_session",           CmdSetSession,           METH_VARARGS },
   {"set_symmetry",          CmdSetCrystal,           METH_VARARGS },
	{"set_title",             CmdSetTitle,             METH_VARARGS },
	{"set_wizard",            CmdSetWizard,            METH_VARARGS },
	{"set_wizard_stack",      CmdSetWizardStack,       METH_VARARGS },
   {"set_view",              CmdSetView,              METH_VARARGS },
   {"set_vis",               CmdSetVis,               METH_VARARGS },
	{"setframe",	           CmdSetFrame,             METH_VARARGS },
	{"showhide",              CmdShowHide,             METH_VARARGS },
	{"slice_new",                 CmdSliceNew,              METH_VARARGS },
	{"smooth",	              CmdSmooth,               METH_VARARGS },
	{"sort",                  CmdSort,                 METH_VARARGS },
   {"spectrum",              CmdSpectrum,             METH_VARARGS },
   {"spheroid",              CmdSpheroid,             METH_VARARGS },
	{"splash",                CmdSplash,               METH_VARARGS },
	{"stereo",	              CmdStereo,               METH_VARARGS },
	{"system",	              CmdSystem,               METH_VARARGS },
	{"symexp",	              CmdSymExp,               METH_VARARGS },
	{"test",	                 CmdTest,                 METH_VARARGS },
	{"toggle",                CmdToggle,               METH_VARARGS },
	{"matrix_transfer",       CmdMatrixTransfer,       METH_VARARGS },
	{"transform_object",      CmdTransformObject,      METH_VARARGS },
	{"transform_selection",   CmdTransformSelection,   METH_VARARGS },
	{"translate_atom",        CmdTranslateAtom,        METH_VARARGS },
	{"translate_object_ttt",  CmdTranslateObjectTTT,   METH_VARARGS },
	{"turn",	                 CmdTurn,                 METH_VARARGS },
	{"viewport",              CmdViewport,             METH_VARARGS },
    {"vdw_fit",               CmdVdwFit,               METH_VARARGS },
	{"undo",                  CmdUndo,                 METH_VARARGS },
	{"unpick",                CmdUnpick,               METH_VARARGS },
	{"unset",                 CmdUnset,                METH_VARARGS },
	{"update",                CmdUpdate,               METH_VARARGS },
	{"window",                CmdWindow,               METH_VARARGS },
	{"zoom",	                 CmdZoom,                 METH_VARARGS },
	{NULL,		              NULL}     /* sentinel */        
};


void init_cmd(void)
{
  Py_InitModule("_cmd", Cmd_methods);
}

#else
typedef int this_file_is_no_longer_empty;
#endif