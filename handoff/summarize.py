import json, sys, collections
d = json.load(open(sys.argv[1]))
res = collections.Counter(); fails = []
def walk(node, path):
    if 'actual' in node:
        a = node['actual'].split()[-1]
        res[a] += 1
        if a not in ('PASS', 'SKIP'):
            fails.append((a, path))
        return
    for k, v in node.items():
        walk(v, path + ('.' if path else '') + k)
walk(d['tests'], '')
total = sum(res.values()); run = total - res['SKIP']
print(f"total={total} run={run} pass={res['PASS']} " + ' '.join(f'{k}={v}' for k, v in sorted(res.items()) if k != 'PASS'))
if '-v' in sys.argv:
    for a, p in sorted(fails): print(a, p)
