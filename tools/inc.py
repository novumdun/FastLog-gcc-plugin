import os
import sys

file_path = os.path.abspath(__file__)
file_dir = os.path.dirname(file_path)
script_dir = os.path.join(file_dir, "../")
sys.path.append(script_dir)