# -*- python -*-
# Git Version: @git@

#-----------------------------------------------------------------------
# XALT: A tool that tracks users jobs and environments on a cluster.
# Copyright (C) 2013-2014 University of Texas at Austin
# Copyright (C) 2013-2014 University of Tennessee
# 
# This library is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of 
# the License, or (at your option) any later version. 
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser  General Public License for more details. 
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 59 Temple Place, Suite 330,
# Boston, MA 02111-1307 USA
#-----------------------------------------------------------------------

from __future__ import print_function
import os, sys, re, argparse

class CmdLineOptions(object):
  """ Command line Options class """

  def __init__(self):
    """ Empty Ctor """
    pass
  
  def execute(self):
    """ Specify command line arguments and parse the command line"""
    parser = argparse.ArgumentParser()
    parser.add_argument("--confFn",      dest='confFn',     action="store", help="python config file")
    parser.add_argument("--xalt_cfg",    dest='xaltCFG',    action="store", help="XALT std config")
    parser.add_argument("--input",       dest='input',      action="store", help="input template file")
    parser.add_argument("--output",      dest='output',     action="store", help="output file")
    args = parser.parse_args()
    
    return args

def convert_template(pattern, replaceA  ,inputFn, outputFn):
  try:
    f = open(inputFn,"r")
  except IOError as e:
    print("Unable to open \"%s\", aborting!" % (inputFn))
    sys.exit(-1)

  
  outA = []
  for line in f:
    idx = line.find(pattern)
    if (idx == -1):
      outA.append(line)
    else:
      outA.append("python_pkg_patternA = [\n")
      
      for entry in replaceA:
        kind = entry['kind'].lower()
        patt = entry['patt'].replace("\\/","/")
        k_s  = entry['k_s'].upper()
        if ( k_s != "KEEP" and k_s != "SKIP"):
          print("Bad k_s value for patt: ",patt)
          sys.exit(-1)
        if ( kind != "name" and kind != "path"):
          print("Bad k_s value for patt: ",patt)
          sys.exit(-1)
        line = "  { 'k_s' : '" + k_s +"', 'kind' : '" + kind + "', 'patt' : re.compile( r'" + patt + "'), 'bare_patt' : r'"+patt+"' },\n"
        outA.append(line)

      outA.append("]\n")

  f.close()
  
  try:
    of = open(outputFn,"w")
  except IOError as e:
    print("Unable to open \"%s\", aborting!" % (outputFn))
    sys.exit(-1)
        
  of.write("".join(outA))
  of.close()

def main():

  my_replacement = "python_pkg_patterns"
  args = CmdLineOptions().execute()
  namespace = {}
  exec(open(args.confFn).read(), namespace)
  replaceA   = namespace.get(my_replacement, [])

  namespace = {}
  exec(open(args.xaltCFG).read(), namespace)
  replaceA.extend(namespace.get(my_replacement, []))

  convert_template("@"+my_replacement+"@", replaceA, args.input, args.output)

if ( __name__ == '__main__'): main()

