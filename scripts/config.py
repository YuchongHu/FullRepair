nks = [(6, 4)]

size = 1 << 6
psize = 1 << 2

recv_thr_num = 20
comp_thr_num = 10
proc_thr_num = 40

mem_num = 30
mem_size = 1 << 26

config_dir = 'config/'
address_file = 'addresses.txt'
algorithm_file = 'algorithms.txt'
bandwidth_file = 'bandwidths.txt'
config_file = 'config.txt'
config_format_file = 'config_format.txt'
task_file = 'tasks.txt'

data_file_dir = 'files/'
result_file = 'results.txt'
rw_file_len = 2
rw_file_fill = 0
rfile_end = '-rfile.txt'
wfile_end = '-wfile.txt'

only_print_net_constrain = True
eth_name = 'eth0'

ips = [('127.0.0.1', 10083),
       ('127.0.0.1', 10084),
       ('127.0.0.1', 10085),
       ('127.0.0.1', 10086),
       ('127.0.0.1', 10087),
       ('127.0.0.1', 10088),
       ('127.0.0.1', 10089),
       ('127.0.0.1', 10090),
       ('127.0.0.1', 10091),
       ('127.0.0.1', 10092),
       ('127.0.0.1', 10093),
       ('127.0.0.1', 10094),
       ('127.0.0.1', 10095),
       ('127.0.0.1', 10096),
       ('127.0.0.1', 10097),
       ('127.0.0.1', 10098),
       ('127.0.0.1', 10099)]

rid = 1
times = 1
alg_dict = {'b': 'ExploitRepair', 'v': 'MutiPipeline', 'e': 'Exr',
            'f': 'PivotRepair', 'p': 'PPT', 'r': 'RP', 'j': 'PPR'}
algs = ['b']
min_bw = 50
best_even_distribute = False
best_BED = 1 if best_even_distribute else 0
exploit_task_num = 3
eva_task_num = 3

config_format = '''\
{size} {psize}

{config_dir + address_file}
{config_dir + bandwidth_file}

{recv_thr_num} {comp_thr_num} {proc_thr_num}
{mem_num} {mem_size}

{config_dir + algorithm_file}
{config_dir + task_file}
{data_file_dir + result_file}

{data_file_dir}
{rw_file_len} {rw_file_fill}
{rfile_end}
{wfile_end}

{if_only_print_net_constrain}
{eth_name}
'''

def write_address_file():
    with open(config_dir + address_file, 'w') as f:
        f.write(f'{len(ips)}\n')
        f.writelines([f'{ip} {port}\n' for ip, port in ips])

def get_alg_string(alg, n, k):
    if alg == 'e':
        return f'{alg} 4 {k} {n} {rid} {exploit_task_num}\n'
    elif alg == 'v':
        return f'{alg} 4 {k} {n} {rid} {eva_task_num}\n'
    elif alg == 'b':
        return f'{alg} 5 {k} {n} {rid} {best_BED} {min_bw}\n'
    elif alg == 'j':
        return f'{alg} 4 {k} {n} {rid} {min_bw}\n'
    else:
        return f'{alg} 4 {k} {n} {rid} {min_bw}\n'

def write_algorithm_file():
    with open(config_dir + algorithm_file, 'w') as f:
        f.write(f'{len(nks) * len(algs) * times}\n')
        for _ in range(times):
            for n, k in nks:
                    for alg in algs:
                        f.write(get_alg_string(alg, n, k))

def write_config_file():
    with open(config_dir + config_file, 'w') as f:
        if_only_print_net_constrain = 1 if only_print_net_constrain else 0
        f.write(eval(f"f'''{config_format}'''"))
    with open(config_dir + config_format_file, 'w') as f:
        f.write(config_format)


if __name__ == '__main__':
    write_address_file()
    write_algorithm_file()
    write_config_file()
