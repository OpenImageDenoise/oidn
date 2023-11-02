#!/usr/bin/env python3

## Copyright 2020 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

import sys
import shutil
from glob import glob
from shutil import which
import argparse

from common import *

BASELINE_VERSION='v2.1.0'

# Parse the command-line arguments
parser = argparse.ArgumentParser(description='Runs all tests, including comparing images produced by the library with generated baseline images.')
parser.usage = '\rIntel(R) Open Image Denoise - Test\n' + parser.format_usage()
parser.add_argument('command', type=str, nargs='?', choices=['baseline', 'run'], default='run')
parser.add_argument('--device', '-d', type=str, default='default', help='device to test')
parser.add_argument('--filter', '-f', type=str, nargs='*', choices=['RT', 'RTLightmap'], default=None, help='filters to test')
parser.add_argument('--build_dir', '-B', type=str, help='build directory')
parser.add_argument('--install_dir', '-I', type=str, help='install directory')
parser.add_argument('--data_dir', '-D', type=str, help='directory of datasets (e.g. training, validation, test)')
parser.add_argument('--results_dir', '-R', type=str, help='directory of training results')
parser.add_argument('--baseline_dir', '-G', type=str, help='directory of generated baseline images')
parser.add_argument('--arch', '-a', type=str, choices=['native', 'pnr', 'hsw', 'skx'], default='native', help='CPU architectures to test (requires Intel SDE)')
parser.add_argument('--minimal', action='store_true', help='run minimal tests')
parser.add_argument('--full', action='store_true', help='run full tests')
parser.add_argument('--log', '-l', type=str, default=None, help='output log file')
cfg = parser.parse_args()

if cfg.minimal and cfg.full:
  print('Error: cannot specify both --minimal and --full')
  exit(1)

training_dir = os.environ.get('OIDN_TRAINING_DIR_' + OS.upper())
if training_dir is None:
  training_dir = os.path.join(root_dir, 'training')
if cfg.data_dir is None:
  cfg.data_dir = os.path.join(training_dir, 'data')
if cfg.results_dir is None:
  cfg.results_dir = os.path.join(training_dir, 'results')
if cfg.baseline_dir is None:
  cfg.baseline_dir = os.path.join(training_dir, 'baseline_' + BASELINE_VERSION)

if cfg.command == 'run':
  # Detect the OIDN binary directory
  if cfg.build_dir is None:
    cfg.build_dir = os.path.join(root_dir, 'build')
  else:
    cfg.build_dir = os.path.abspath(cfg.build_dir)

  if cfg.install_dir is None:
    cfg.install_dir = os.path.join(root_dir, 'install')
  else:
    cfg.install_dir = os.path.abspath(cfg.install_dir)

  bin_dir = os.path.join(cfg.install_dir, 'bin')
  if not os.path.isdir(bin_dir):
    bin_dir = cfg.build_dir

  # Detect the Intel(R) Software Development Emulator (SDE)
  # See: https://software.intel.com/en-us/articles/intel-software-development-emulator
  sde = 'sde.exe' if OS == 'windows' else 'sde64'
  sde_dir = os.environ.get('OIDN_SDE_DIR_' + OS.upper())
  if sde_dir is not None:
    sde = os.path.join(sde_dir, sde)

# Prints the name of a test
def print_test(name, kind='Test'):
  print(kind + ':', name, '...', end=('' if cfg.log else None), flush=True)

# Runs a test command
def run_test(cmd, arch='native'):
  # Run test through SDE if required
  if arch != 'native':
    cmd = f'{sde} -{arch} -- ' + cmd

  # Write command and redirect output to log
  if cfg.log:
    run(f'echo >> "{cfg.log}"')
    run(f'echo "{cmd}" >> "{cfg.log}"')
    cmd += f' >> "{cfg.log}" 2>&1'
  else:
    print(f'Command: {cmd}')

  # Run the command and check the return value
  if os.system(cmd) == 0:
    if cfg.log:
      print(' PASSED')
    else:
      print('PASSED\n')
  else:
    if cfg.log:
      print(' FAILED')
    else:
      print('FAILED\n')
    if cfg.log:
      print(f'Error: test failed, see "{cfg.log}" for details')
    else:
      print('Error: test failed')
    exit(1)

# Runs main tests
def test():
  if cfg.command == 'run':
    print_test('oidnTest')
    test_cmd = os.path.join(bin_dir, 'oidnTest')
    if cfg.device != 'default':
      test_cmd += f' --device {cfg.device}'
    run_test(test_cmd, cfg.arch)

    if not cfg.minimal:
      print_test('oidnBenchmark')
      test_cmd = os.path.join(bin_dir, 'oidnBenchmark -v 1')
      if cfg.device != 'default':
        test_cmd += f' --device {cfg.device}'
      run_test(test_cmd, cfg.arch)

# Gets the option name of a feature
def get_feature_opt(feature):
  if feature == 'calb':
    return 'alb'
  elif feature == 'cnrm':
    return 'nrm'
  else:
    return feature

# Gets the file extension of a feature
def get_feature_ext(feature):
  if feature == 'dir':
    return 'sh1x'
  else:
    return get_feature_opt(feature)

# Runs regression tests for the specified filter
def test_regression(filter, feature_sets, dataset):
  dataset_dir  = os.path.join(cfg.data_dir, dataset)
  baseline_dir = os.path.join(cfg.baseline_dir, dataset)

  # Convert the input images to PFM
  if cfg.command == 'baseline':
    if os.path.exists(baseline_dir):
      print('Error: baseline directory already exists')
      exit(1)
    os.makedirs(baseline_dir)

    input_filenames = sorted(glob(os.path.join(dataset_dir, '**', '*.exr'), recursive=True))
    for input_filename in input_filenames:
      image_name, feature = os.path.relpath(input_filename, dataset_dir).rsplit('.', 2)[0:2]
      print_test(f'{filter}.{image_name}.{feature}', 'Convert')
      output_filename = os.path.join(baseline_dir, f'{image_name}.input.{feature}.pfm')
      convert_cmd = os.path.join(root_dir, 'training', 'convert_image.py')
      convert_cmd += f' "{input_filename}" "{output_filename}"'
      run_test(convert_cmd)

  # Iterate over the feature sets
  for features, full_test in feature_sets:
    # Get the result name
    result = filter.lower()
    for f in features:
      result += '_' + f
    features_str = result.split('_', 1)[1]

    if cfg.command == 'baseline':
      # Generate the baseline images
      print_test(f'{filter}.{features_str}', 'Infer')
      infer_cmd = os.path.join(root_dir, 'training', 'infer.py')
      infer_cmd += f' -D "{cfg.data_dir}" -R "{cfg.results_dir}" -O "{cfg.baseline_dir}" -i {dataset} -r {result} -F pfm'
      run_test(infer_cmd)

    elif cfg.command == 'run':
      main_feature = features[0]
      main_feature_ext = get_feature_ext(main_feature)

      # Gather the list of input images
      input_filenames = sorted(glob(os.path.join(baseline_dir, '**', f'*.input.{main_feature_ext}.pfm'), recursive=True))
      if not input_filenames:
        print('Error: baseline input images missing (run with "baseline" first)')
        exit(1)
      image_names = [os.path.relpath(filename, baseline_dir).rsplit('.', 3)[0] for filename in input_filenames]

      # Iterate over quality
      for quality in (['high', 'balanced'] if filter == 'RT' or cfg.full else ['high']):
        # Iterate over images
        for image_name in image_names:
          # Iterate over precision
          for precision in (['fp32', 'fp16'] if full_test or cfg.full else ['fp32']):
            # Iterate over in-place mode
            for inplace in ([False, True] if full_test or cfg.full else [False]):
              # Iterate over memory usage
              for maxmem in ([None, 200] if full_test or cfg.full else [None]):
                # Run test
                test_name = f'{filter}.{quality}.{features_str}.{image_name}.{precision}'
                if inplace:
                  test_name += '.inplace'
                if maxmem:
                  test_name += f'.maxmem{maxmem}'

                denoise_cmd = os.path.join(bin_dir, 'oidnDenoise')
                if cfg.device != 'default':
                  denoise_cmd += f' --device {cfg.device}'
                denoise_cmd += f' -f {filter} -q {quality}'

                features_exist = True
                for feature in features:
                  feature_opt = get_feature_opt(feature)
                  feature_ext = get_feature_ext(feature)
                  feature_filename = os.path.join(baseline_dir, f'{image_name}.input.{feature_ext}.pfm')
                  if not os.path.isfile(feature_filename):
                    features_exist = False
                    break
                  denoise_cmd += f' --{feature_opt} "{feature_filename}"'

                if not features_exist:
                  continue
                print_test(test_name)

                ref_filename = os.path.join(baseline_dir, f'{image_name}.{result}.{main_feature_ext}.pfm')
                if not os.path.isfile(ref_filename):
                  print('Error: baseline output image missing (run with "baseline" first)')
                  exit(1)
                denoise_cmd += f' --ref "{ref_filename}" -n 3 -v 2'

                if set(features) & {'calb', 'cnrm'}:
                  denoise_cmd += ' --clean_aux'

                if precision == 'fp16':
                  denoise_cmd += ' -t half'

                if inplace:
                  denoise_cmd += ' --inplace'

                if maxmem:
                  denoise_cmd += f' --maxmem {maxmem}'

                run_test(denoise_cmd, cfg.arch)

# Main tests
test()

if not cfg.minimal:
  # Regression tests: RT
  if not cfg.filter or 'RT' in cfg.filter:
    test_regression(
      'RT',
      [
        (['hdr', 'alb', 'nrm'],   True),
        (['hdr', 'alb'],          False),
        (['hdr'],                 True),
        (['hdr', 'calb', 'cnrm'], False),
        (['ldr', 'alb', 'nrm'],   False),
        (['ldr', 'alb'],          False),
        (['ldr'],                 True),
        (['ldr', 'calb', 'cnrm'], False),
        (['alb'],                 True),
        (['nrm'],                 True)
      ],
      'rt_regress'
    )

  # Regression tests: RTLightmap
  if not cfg.filter or 'RTLightmap' in cfg.filter:
    test_regression(
      'RTLightmap',
      [
        (['hdr'], True),
        (['dir'], False)
      ],
      'rtlightmap_regress'
    )

# Done
if cfg.command == 'run':
  print('Success: all tests passed')