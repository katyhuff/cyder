import os
from subprocess import call
import fileinput, sys
import re
from argparse import ArgumentParser
from optparse import OptionParser
import numpy as np
from decimal import Decimal

def make_dir(dir_path) :
    """ makes a directory at path/name/"""
    dpath, dname = os.path.splitext(dir_path)
    directory = full_path(dpath, dname)
    if not os.path.exists(directory):
        os.makedirs(directory)

def configure_infile(xml_in, xml, param, val) :
    """ copies the in_file to out_file, then changes param to val """
    f_old = open(xml_in, 'r') 
    f_new = open(xml, 'w') 
    old = "<"+param+">[^<]*</"+param+">"
    new = "<"+param+">"+str(Decimal(val))+"</"+param+">"
    for line in f_old :
        f_new.write(re.sub(old, new, line))
    f_old.close()
    f_new.close()
    return xml
     
def configure_infiles(xml_in, out_path, param, val_list) :
    """ takes a list of values, each of which will result in runnable xml. """
    xml_list = []
    for val in val_list:
        xml = out_path+"/"+change_extension(xml_in, "_"+str(val)+".xml")
        xml_list.append(configure_infile(xml_in, xml, param, val))
    return xml_list

def run_cyclus(in_file_list, out_path) :
    """ for each infile in the dict, run cyclus and create the outfile in the dict"""
    for in_file in in_file_list : 
        out_file = full_path(out_path, strip_xml(in_file)+".sqlite")
        call(["/usr/local/cyclus/bin/cyclus", in_file, "-o", out_file])

def strip_xml(full_path) :
    """ returns root of the filename. /usr/local/cyclus.xml returns cyclus """
    fpath, fname = os.path.split(full_path)
    return re.sub('.xml$', '', fname)

def strip_in(full_path) :
    """ returns root of the filename. /usr/local/cyclus.xml.in returns cyclus.xml """
    fpath, fname = os.path.split(full_path)
    return re.sub('.in$', '', fname)

def full_path(fpath, fname) :
    """ for a path and a name, create a full path."""
    to_ret = os.path.abspath(os.path.join(fpath, fname))
    return to_ret

def change_extension(fname, new_ext) :
    """ returns filename or full path with new extension """
    root_xml = strip_in(fname)
    root = strip_xml(root_xml)
    return root+new_ext

def make_param_range(low, upper, number, logspace):
    """ makes a range between low and high with number vals"""
    if logspace == True :
        arr = np.logspace(np.log10(float(low)), np.log10(float(upper)), int(number))
    else :
        arr = np.linspace(float(low), float(upper), int(number))
    return arr.tolist()

def perturb(args) :
    make_dir(args.out_path[0])
    in_file_list = args.xml_in
    for param in args.params :
        ind = args.params.index(param)
        param_range = make_param_range(args.lows[ind], args.uppers[ind], 
                args.numbers[ind], bool(args.logspace))
        curr_list = list(in_file_list)
        in_file_list = []
        for f in curr_list : 
            in_file_list.extend(configure_infiles(f, args.out_path[0], param, 
                param_range))
    in_file_list = [item for item in in_file_list if not item.endswith("xml.in")]
    run_cyclus(in_file_list, args.out_path[0])

def main() :
    arg_parser = ArgumentParser(description="Perturb 1 or 2 variables and run "
            "cyclus")
    arg_parser.add_argument("-i", metavar="infile", type=str, nargs=1, dest="xml_in",
                      help="read data from foo.xml.in")
    arg_parser.add_argument("-o", "--out_path", type=str, nargs=1, dest="out_path",
                      help="place sqlite files in out_path")
    arg_parser.add_argument("-p", "--param", type=str, nargs='*', dest="params",
                      help="the parameter to perturb")
    arg_parser.add_argument("-l", "--low", type=float, nargs='*', dest="lows",
                      help="the low value of the param range")
    arg_parser.add_argument("-u", "--upper", type=float, nargs='*',dest="uppers",
                      help="the upper value of the param range")
    arg_parser.add_argument("-n", "--num", type=int, nargs='*', dest="numbers",
                      help="number of values of the param range")
    arg_parser.add_argument("-ls", "--logspace", type=str, nargs=1, dest="logspace",
                      help="true if you want to logspace the parametric spacing.")
    args = arg_parser.parse_args()
    perturb(args)


if __name__ == "__main__":
    main()
