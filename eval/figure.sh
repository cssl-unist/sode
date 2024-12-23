#!/bin/bash

mkdir -p figure/data/

python parser.py

cd figure
python fig5_a.py
python fig5_b.py
python fig5_c.py
python fig5_d.py

python fig6_a.py
python fig6_b.py
python fig6_c.py

python fig7_a.py
python fig7_b.py

python fig8_a.py
python fig8_b.py

python fig9.py

python fig10.py

python fig11.py

python fig12.py

python fig13.py

python appendix_fig2.py
cd ..
