import json, sys
def load(p):
    d = json.load(open(p)); r = {}
    def walk(n, path):
        if 'actual' in n: r[path] = n['actual'].split()[-1]; return
        for k, v in n.items(): walk(v, path + ('.' if path else '') + k)
    walk(d['tests'], ''); return r
a, b = load(sys.argv[1]), load(sys.argv[2])
reg = [t for t in a if a[t] == 'PASS' and b.get(t) not in ('PASS', None)]
print('regressions:', len(reg))
for t in sorted(reg): print('  ', b[t], t)
if '-f' in sys.argv:
    for t in sorted(b):
        if b[t] not in ('PASS', 'SKIP'): print('  still', b[t], t)
