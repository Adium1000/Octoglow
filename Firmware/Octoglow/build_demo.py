# -*- coding: utf-8 -*-
"""portal.html + backend fals = portal_demo.html

   Interfata reala, dar cu demo_mock.js injectat in <head> ca window.fetch sa
   fie inlocuit inainte sa porneasca scripturile paginii. Rezultatul e un
   singur fisier care merge oriunde - deschis local, servit in retea sau pus
   pe un link - fara ceas si fara backend.

   Ruleaza-l din nou dupa fiecare modificare in portal.html; nu se editeaza
   nimic in fisierul original.

     python build_demo.py
"""
import os
import sys
import io

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, 'portal.html')
MOCK = os.path.join(HERE, 'demo_mock.js')
OUT = os.path.join(HERE, 'portal_demo.html')

src = open(SRC, encoding='utf-8', newline='').read()
mock = open(MOCK, encoding='utf-8', newline='').read()

# 1. Scriptul intra imediat dupa <title>, deci in <head>: ruleaza inaintea
#    oricarui script al paginii, care sunt toate la finalul documentului.
anchor = '<title>Octoglow</title>'
assert src.count(anchor) == 1, 'ancora <title> nu e unica'
src = src.replace(anchor, anchor + '\n<script>\n' + mock + '</script>', 1)

# 2. Iesirile catre "/" (delogare, reset din fabrica, sesiune expirata) ar
#    parasi pagina si ar duce la un 404. In demo nu exista unde sa mearga.
n = src.count('window.location.href = `/`')
assert n > 0
src = src.replace('window.location.href = `/`', '__demoNav()')

open(OUT, 'w', encoding='utf-8', newline='').write(src)
print('portal_demo.html scris  ({:,} octeti)'.format(len(src.encode('utf-8'))))
print('  mock injectat          {:,} octeti'.format(len(mock.encode('utf-8'))))
print('  navigari neutralizate  {}'.format(n))
