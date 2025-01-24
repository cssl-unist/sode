#!/bin/bash

mkdir -p figure/data/

python3 parser.py

cd figure
python3 fig5_a.py
python3 fig5_b.py
python3 fig5_c.py
python3 fig5_d.py

python3 fig6_a.py
python3 fig6_b.py
python3 fig6_c.py

python3 fig7_a.py
python3 fig7_b.py

python3 fig8_a.py
python3 fig8_b.py

python3 fig9.py

python3 fig10.py

python3 fig11.py

python3 fig12.py

python3 fig13.py

python3 appendix_fig2.py
cd ..
