/**
 * Copyright (C) 2010-2012 Thalmann Software & Consulting, http://www.softdev.ch
 *
 * This file is part of ht4w.
 *
 * ht4w is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or any later version.
 *
 * Hypertable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef _WIN32
#error Platform isn't supported
#endif

#include "Common/Compat.h"
#include "Common/Config.h"
#include "Common/Logger.h"
#include "Common/Serialization.h"
#include "Common/SystemInfo.h"
#include "Common/Stopwatch.h"
#include "Common/Mutex.h"

#include <iphlpapi.h>
#include <shellapi.h>
#include <psapi.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "psapi.lib")

using namespace Hypertable;

bool CpuInfo::operator==(const CpuInfo &other) const {
  if (vendor == other.vendor &&
      model == other.model &&
      mhz == other.mhz &&
      cache_size == other.cache_size &&
      total_sockets == other.total_sockets &&
      total_cores == other.total_cores &&
      cores_per_socket == other.cores_per_socket)
    return true;
  return false;
}

bool CpuStat::operator==(const CpuStat &other) const {
  if (Serialization::equal(user, other.user) &&
      Serialization::equal(sys, other.sys) &&
      Serialization::equal(nice, other.nice) &&
      Serialization::equal(idle, other.idle) &&
      Serialization::equal(wait, other.wait) &&
      Serialization::equal(irq, other.irq) &&
      Serialization::equal(soft_irq, other.soft_irq) &&
      Serialization::equal(stolen, other.stolen) &&
      Serialization::equal(total, other.total))
    return true;
  return false;
}

bool LoadAvgStat::operator==(const LoadAvgStat &other) const {
  if (Serialization::equal(loadavg[0], other.loadavg[0]) &&
      Serialization::equal(loadavg[1], other.loadavg[1]) &&
      Serialization::equal(loadavg[2], other.loadavg[2]))
    return true;
  return false;
}

bool MemStat::operator==(const MemStat &other) const {
  if (Serialization::equal(ram, other.ram) &&
      Serialization::equal(total, other.total) &&
      Serialization::equal(used, other.used) &&
      Serialization::equal(free, other.free) &&
      Serialization::equal(actual_used, other.actual_used) &&
      Serialization::equal(actual_free, other.actual_free))
    return true;
  return false;
}

bool DiskStat::operator==(const DiskStat &other) const {
  if (prefix == other.prefix &&
      Serialization::equal(reads_rate, other.reads_rate) &&
      Serialization::equal(writes_rate, other.writes_rate) &&
      Serialization::equal(read_rate, other.read_rate) &&
      Serialization::equal(write_rate, other.write_rate))
    return true;
  return false;
}

bool SwapStat::operator==(const SwapStat &other) const {
  if (Serialization::equal(total, other.total) &&
      Serialization::equal(used, other.used) &&
      Serialization::equal(free, other.free) &&
      page_in == other.page_in &&
      page_out == other.page_out)
    return true;
  return false;
}

bool NetInfo::operator==(const NetInfo &other) const {
  if (host_name == other.host_name &&
      primary_if == other.primary_if &&
      primary_addr == other.primary_addr &&
      default_gw == other.default_gw)
    return true;
  return false;
}

bool NetStat::operator==(const NetStat &other) const {
  if (tcp_established == other.tcp_established &&
      tcp_listen == other.tcp_listen &&
      tcp_time_wait == other.tcp_time_wait &&
      tcp_close_wait == other.tcp_close_wait &&
      tcp_idle == other.tcp_idle &&
      Serialization::equal(rx_rate, other.rx_rate) &&
      Serialization::equal(tx_rate, other.tx_rate))
    return true;
  return false;
}

bool OsInfo::operator==(const OsInfo &other) const {
  if (name == other.name &&
      version == other.version &&
      version_major == other.version_major &&
      version_minor == other.version_minor &&
      version_micro == other.version_micro &&
      arch == other.arch &&
      machine == other.machine &&
      description == other.description &&
      patch_level == other.patch_level &&
      vendor == other.vendor &&
      vendor_version == other.vendor_version &&
      vendor_name == other.vendor_name &&
      code_name == other.code_name)
    return true;
  return false;
}

bool ProcInfo::operator==(const ProcInfo &other) const {
  if (pid == other.pid &&
      user == other.user &&
      exe == other.exe &&
      cwd == other.cwd &&
      root == other.root &&
      args == other.args)
    return true;
  return false;
}

bool ProcStat::operator==(const ProcStat &other) const {
  if (cpu_user == other.cpu_user &&
      cpu_sys == other.cpu_sys &&
      cpu_total == other.cpu_total &&
      Serialization::equal(cpu_pct, other.cpu_pct) &&
      Serialization::equal(vm_size, other.vm_size) &&
      Serialization::equal(vm_resident, other.vm_resident) &&
      Serialization::equal(vm_share, other.vm_share) &&
      minor_faults == other.minor_faults &&
      major_faults == other.major_faults &&
      page_faults == other.page_faults)
    return true;
  return false;
}

bool FsStat::operator==(const FsStat &other) const {
  if (prefix == other.prefix &&
      total == other.total &&
      free == other.free &&
      used == other.used &&
      avail == other.avail &&
      Serialization::equal(use_pct, other.use_pct) &&
      files == other.files &&
      free_files == other.free_files)
    return true;
  return false;
}

bool TermInfo::operator==(const TermInfo &other) const {
  if (term == other.term &&
      num_lines == other.num_lines &&
      num_cols == other.num_cols)
    return true;
  return false;
}

namespace {

RecMutex _mutex;

// for unit conversion to/from MB/GB
const double KiB = 1024.;
const double MiB = KiB * 1024;
const double GiB = MiB * 1024;

// info/stat singletons
CpuInfo _cpu_info, *_cpu_infop = NULL;
CpuStat _cpu_stat;
LoadAvgStat _loadavg_stat;
MemStat _mem_stat;
DiskStat _disk_stat;
OsInfo _os_info, *_os_infop = NULL;
SwapStat _swap_stat;
NetInfo _net_info, *_net_infop = NULL;
NetStat _net_stat;
ProcInfo _proc_info, *_proc_infop = NULL;
ProcStat _proc_stat;
FsStat _fs_stat;
TermInfo _term_info, *_term_infop = NULL;

class ProcessTimesPerfmon {
  public:
    ProcessTimesPerfmon()
      : prev_cpu_total(0), prev_refresh(0) {
    }

    virtual ~ProcessTimesPerfmon() {
    }

    void refresh(uint64_t& cpu_user, uint64_t& cpu_sys, uint64_t& cpu_total, double& cpu_pct) {
      FILETIME creationTime, exitTime, kernelTime, userTime, refreshTime;
      HT_ASSERT(GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime));
      GetSystemTimeAsFileTime(&refreshTime);
      cpu_user = (((uint64_t)userTime.dwHighDateTime << 32) | userTime.dwLowDateTime) / 1000;
      cpu_sys = (((uint64_t)userTime.dwHighDateTime << 32) | userTime.dwLowDateTime) / 1000;
      cpu_total = cpu_user + cpu_sys;
      uint64_t refresh = (((uint64_t)refreshTime.dwHighDateTime << 32) | refreshTime.dwLowDateTime) / 1000;
      cpu_pct = prev_refresh ? double(cpu_total - prev_cpu_total) / double(refresh - prev_refresh) * 100.0 : 0.0;
      prev_cpu_total = cpu_total;
      prev_refresh = refresh;
    }

  private:

   uint64_t prev_refresh;
   uint64_t prev_cpu_total;
};
ProcessTimesPerfmon _processTimesPerfmon;


} // local namespace

namespace Hypertable {

const CpuInfo &System::cpu_info() {
  ScopedRecLock lock(_mutex);

  if (!_cpu_infop)
    _cpu_infop = &_cpu_info.init();

  return _cpu_info;
}

const CpuStat &System::cpu_stat() {
  return _cpu_stat.refresh();
}

const LoadAvgStat &System::loadavg_stat() {
  return _loadavg_stat.refresh();
}

const MemStat &System::mem_stat() {
  return _mem_stat.refresh();
}

const DiskStat &System::disk_stat() {
  return _disk_stat.refresh();
}

const OsInfo &System::os_info() {
  ScopedRecLock lock(_mutex);

  if (!_os_infop)
    _os_infop = &_os_info.init();

  return _os_info;
}

const SwapStat &System::swap_stat() {
  return _swap_stat.refresh();
}

const NetInfo &System::net_info() {
  ScopedRecLock lock(_mutex);
  if (!_net_infop)
    _net_infop = &_net_info.init();

  return _net_info;
}

const NetStat &System::net_stat() {
  return _net_stat.refresh();
}

const ProcInfo &System::proc_info() {
  ScopedRecLock lock(_mutex);

  if (!_proc_infop)
    _proc_infop = &_proc_info.init();

  return _proc_info;
}

const ProcStat &System::proc_stat() {
  return _proc_stat.refresh();
}

const FsStat &System::fs_stat() {
  return _fs_stat.refresh();
}

const TermInfo &System::term_info() {
  ScopedRecLock lock(_mutex);

  if (!_term_infop)
    _term_infop = &_term_info.init();

  return _term_info;
}

CpuInfo &CpuInfo::init() {
  ScopedRecLock lock(_mutex);

  vendor.clear();
  model.clear();
  cache_size = 0;
  total_sockets = 0;
  cores_per_socket = 0;

  SYSTEM_INFO systemInfo;
  ZeroMemory(&systemInfo, sizeof(systemInfo));
  ::GetSystemInfo(&systemInfo);
  total_cores = systemInfo.dwNumberOfProcessors;

  mhz = -1;
  HKEY hkCentralProcessor;
  char id[MAX_PATH + 1];
  if (RegOpenKeyA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor", &hkCentralProcessor) == ERROR_SUCCESS) {
    if (RegEnumKeyA(hkCentralProcessor, 0, id, sizeof(id)) == ERROR_SUCCESS) {
      HKEY hkCpu;
      if (RegOpenKeyA(hkCentralProcessor, id, &hkCpu) == ERROR_SUCCESS) {
        DWORD _mhz;
        DWORD size = sizeof(_mhz);
        if (RegQueryValueExA(hkCpu, "~MHz", 0, 0, (LPBYTE)&_mhz, &size) == ERROR_SUCCESS) {
          mhz = _mhz;
        }
        RegCloseKey(hkCpu);
      }
    }
    RegCloseKey(hkCentralProcessor);
  }

  return *this;
}

CpuStat &CpuStat::refresh() {
  ScopedRecLock lock(_mutex);

  user = 0;
  sys = 0;
  nice = 0;
  idle = 0;
  wait = 0;
  irq = 0;
  soft_irq = 0;
  stolen = 0;
  total = 0;

  HT_FATAL("Not implemented");
  return *this;
}

LoadAvgStat &LoadAvgStat::refresh() {
  ScopedRecLock lock(_mutex);

  loadavg[0] = 0;
  loadavg[1] = 0;
  loadavg[2] = 0;

  return *this;
}

MemStat &MemStat::refresh() {
  ScopedRecLock lock(_mutex);

  MEMORYSTATUSEX memStatusEx;
  ZeroMemory(&memStatusEx, sizeof(memStatusEx));
  memStatusEx.dwLength = sizeof(memStatusEx);
  if (GlobalMemoryStatusEx(&memStatusEx)) {
    total = memStatusEx.ullTotalPhys / MiB;
    free  = memStatusEx.ullAvailPhys / MiB;
  }
  else {
    MEMORYSTATUS memStatus;
    memStatus.dwLength = sizeof(memStatus);
    ZeroMemory(&memStatus, sizeof(memStatus));
    GlobalMemoryStatus(&memStatus);
    total = memStatus.dwTotalPhys / MiB;
    free  = memStatus.dwAvailPhys / MiB;
  }
  used = total - free;

  ram = total;
  actual_free = free;
  actual_used = used;

  return *this;
}

DiskStat::~DiskStat() {
}

DiskStat &DiskStat::refresh(const char *dir_prefix) {
  ScopedRecLock lock(_mutex);

  prefix = dir_prefix;
  reads_rate = 0;
  writes_rate = 0;
  read_rate = 0;
  write_rate = 0;

  HT_FATAL("Not implemented");
  return *this;
}

SwapStat::~SwapStat() {
}

SwapStat &SwapStat::refresh() {
  ScopedRecLock lock(_mutex);

  total = 0;
  used = 0;
  free = 0;
  page_in = 0;
  page_out = 0;

  HT_FATAL("Not implemented");
  return *this;
}

OsInfo &OsInfo::init() {
  ScopedRecLock lock(_mutex);

  arch.clear();
  machine.clear();
  description.clear();
  patch_level.clear();
  vendor.clear();
  vendor_version.clear();
  vendor_name.clear();

  HT_FATAL("Not implemented");
  return *this;
}

NetInfo &NetInfo::init() {
  ScopedRecLock lock(_mutex);

  host_name.clear();
  primary_addr.clear();
  primary_if.clear();
  default_gw.clear();

  String ifname;
  if (Hypertable::Config::has("Hypertable.Network.Interface"))
    ifname = Hypertable::Config::get_str("Hypertable.Network.Interface");

  ULONG len;
  if (GetNetworkParams(0, &len) == ERROR_BUFFER_OVERFLOW) {
    FIXED_INFO* info = (FIXED_INFO*)malloc(len);
    if (info) {
      ZeroMemory(info, len);
      if (GetNetworkParams(info, &len) == ERROR_SUCCESS) {
        host_name = info->HostName;
      }
      free(info);
    }
  }

  const DWORD adapteraddrs_flags(GAA_FLAG_SKIP_DNS_SERVER|GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST);
  if (GetAdaptersAddresses(AF_INET, adapteraddrs_flags, 0, 0, &len) == ERROR_BUFFER_OVERFLOW) {
    PIP_ADAPTER_ADDRESSES adapteraddrs = (PIP_ADAPTER_ADDRESSES)malloc(len);
    if (adapteraddrs) {
      ZeroMemory(adapteraddrs, len);
      if (GetAdaptersAddresses(AF_INET, adapteraddrs_flags, 0, adapteraddrs, &len) == ERROR_SUCCESS) {
        char buf[1024];
        PIP_ADAPTER_ADDRESSES adapteraddr = adapteraddrs;
        while (adapteraddr && primary_addr.empty()) {
          if (adapteraddr->PhysicalAddressLength &&
              adapteraddr->IfType == IF_TYPE_ETHERNET_CSMACD &&
             (adapteraddr->Flags & IP_ADAPTER_RECEIVE_ONLY) == 0) {
            if (!ifname.empty()) {
              WideCharToMultiByte(CP_ACP, 0, adapteraddr->FriendlyName, -1, buf, sizeof(buf), 0, 0);
              if (stricmp(ifname.c_str(), buf)) {
                adapteraddr = adapteraddr->Next;
                continue;
              }
            }
            PIP_ADAPTER_UNICAST_ADDRESS unicastaddr = adapteraddr->FirstUnicastAddress;
            while (unicastaddr && primary_addr.empty()) {
              if (unicastaddr->Flags & IP_ADAPTER_ADDRESS_DNS_ELIGIBLE) {
                len = sizeof(buf);
                if (WSAAddressToStringA(unicastaddr->Address.lpSockaddr, unicastaddr->Address.iSockaddrLength, 0, buf, &len) == ERROR_SUCCESS) {
                  primary_addr = buf;
                  WideCharToMultiByte(CP_ACP, 0, adapteraddr->FriendlyName, -1, buf, sizeof(buf), 0, 0);
                  primary_if = buf;
                  if (GetAdaptersInfo(0, &len) == ERROR_BUFFER_OVERFLOW) {
                    PIP_ADAPTER_INFO adapters = (PIP_ADAPTER_INFO)malloc(len);
                    if (adapters) {
                      ZeroMemory(adapters, len);
                      if (GetAdaptersInfo(adapters, &len) == ERROR_SUCCESS) {
                        PIP_ADAPTER_INFO adapter = adapters;
                        while (adapter && default_gw.empty()) {
                          if (adapter->Index == adapteraddr->IfIndex) {
                            default_gw = adapter->GatewayList.IpAddress.String;
                          }
                          adapter = adapter->Next;
                        }
                      }
                      free(adapters);
                    }
                  }
                }
              }
              unicastaddr = unicastaddr->Next;
            }
          }
          adapteraddr = adapteraddr->Next;
        }
      }
      free(adapteraddrs);
    }
  }

  if (!ifname.empty() && primary_addr.empty())
    HT_FATALF("Unable to find network interface '%s'", ifname.c_str());

  if (primary_addr.empty())
    primary_addr = "127.0.0.1";
  return *this;
}

NetStat &NetStat::refresh() {
  ScopedRecLock lock(_mutex);

  tcp_established = 0;
  tcp_listen = 0;
  tcp_time_wait = 0;
  tcp_close_wait = 0;
  tcp_idle = 0;
  rx_rate = 0;
  tx_rate = 0;

  HT_FATAL("Not implemented");
  return *this;
}

ProcInfo &ProcInfo::init() {
  ScopedRecLock lock(_mutex);

  pid = GetCurrentProcessId();
  char buf[4096];
  DWORD len = sizeof(buf);
  HT_ASSERT(::GetUserNameA(buf, &len));
  user = buf;
  HT_ASSERT(::GetModuleFileNameA(0, buf, sizeof(buf)));
  *buf = toupper(*buf);
  exe = buf;
  HT_ASSERT(::GetCurrentDirectory(sizeof(buf), buf));
  *buf = toupper(*buf);
  cwd = buf;
  root = cwd.substr(0, 3);

  int argn;
  LPWSTR* argv = CommandLineToArgvW(::GetCommandLineW(), &argn);
  if (argv) {
    for (int i = 0; i < argn; ++i) {
      WideCharToMultiByte(CP_ACP, 0, argv[i], -1, buf, sizeof(buf), 0, 0);
      args.push_back(buf);
    }
    LocalFree(argv);
  }

  return *this;
}

ProcStat &ProcStat::refresh() {
  ScopedRecLock lock(_mutex);

  vm_size = 0;
  vm_resident = 0;
  vm_share = 0;
  minor_faults = 0;
  major_faults = 0;
  page_faults = 0;

  _processTimesPerfmon.refresh(cpu_user, cpu_sys, cpu_total, cpu_pct);

  return *this;
}

FsStat &FsStat::refresh(const char *dir_prefix) {
  ScopedRecLock lock(_mutex);

  use_pct = 0;
  total = 0;
  free = 0;
  used = 0;
  avail = 0;
  files = 0;
  free_files = 0;

  HT_FATAL("Not implemented");
  return *this;
}

TermInfo &TermInfo::init() {
  ScopedRecLock lock(_mutex);
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if(GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
    term = "winnt";
    num_lines = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    num_cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
  }
  else {
    term = "dumb";
    num_lines = 24;
    num_cols = 80;
  }
  return *this;
}

const char *system_info_lib_version() {
  return "SystemInfoWin";
}

std::ostream &system_info_lib_version(std::ostream &out) {
  out <<"{SigarVersion: description='"<<system_info_lib_version() <<'}';
  return out;
}

std::ostream &operator<<(std::ostream &out, const CpuInfo &i) {
  out <<"{CpuInfo: vendor='"<< i.vendor <<"' model='"<< i.model
      <<"' mhz="<< i.mhz <<" cache_size="<< i.cache_size
      <<"\n total_sockets="<< i.total_sockets <<" total_cores="<< i.total_cores
      <<" cores_per_socket="<< i.cores_per_socket <<'}';
  return out;
}

std::ostream &operator<<(std::ostream &out, const CpuStat &s) {
  out <<"{CpuStat: user="<< s.user <<" sys="<< s.sys <<" nice="<< s.nice
      <<" idle="<< s.idle <<" wait="<< s.wait <<"\n irq="<< s.irq
      <<" soft_irq="<< s.soft_irq <<" stolen="<< s.stolen
      <<" total="<< s.total <<'}';
  return out;
}

std::ostream &operator<<(std::ostream &out, const MemStat &s) {
  out <<"{MemStat: ram="<< s.ram <<" total="<< s.total <<" used="<< s.used
      <<" free="<< s.free <<"\n actual_used="<< s.actual_used
      <<" actual_free="<< s.actual_free <<'}';
  return out;
}

std::ostream &operator<<(std::ostream &out, const DiskStat &s) {
  out <<"{DiskStat: prefix='"<< s.prefix <<"' reads_rate="<< s.reads_rate
      <<" writes_rate="<< s.writes_rate <<"\n read_rate=" << s.read_rate
      <<" write_rate="<< s.write_rate <<'}';
  return out;
}

std::ostream &operator<<(std::ostream &out, const OsInfo &i) {
  out <<"{OsInfo: name="<< i.name <<" version="<< i.version <<" arch="<< i.arch
      <<" machine="<< i.machine <<"\n description='"<< i.description
      <<"' patch_level="<< i.patch_level <<"\n vendor="<< i.vendor
      <<" vendor_version="<< i.vendor_version <<" vendor_name="<< i.vendor_name
      <<" code_name="<< i.code_name <<'}';
  return out;
}

std::ostream &operator<<(std::ostream &out, const SwapStat &s) {
  out <<"{SwapStat: total="<< s.total <<" used="<< s.used <<" free="<< s.free
      <<" page_in="<< s.page_in <<" page_out="<< s.page_out <<'}';
  return out;
}

std::ostream &operator<<(std::ostream &out, const NetInfo &i) {
  out <<"{NetInfo: host_name="<< i.host_name <<" primary_if="
      << i.primary_if <<"\n primary_addr="<< i.primary_addr
      <<" default_gw="<< i.default_gw <<'}';
  return out;
}

std::ostream &operator<<(std::ostream &out, const NetStat &s) {
  out <<"{NetStat: rx_rate="<< s.rx_rate <<" tx_rate="<< s.tx_rate
      <<" tcp_established="<< s.tcp_established <<" tcp_listen="
      << s.tcp_listen <<"\n tcp_time_wait="<< s.tcp_time_wait
      <<" tcp_close_wait="<< s.tcp_close_wait <<'}';
  return out;
}

std::ostream &operator<<(std::ostream &out, const ProcInfo &i) {
  out <<"{ProcInfo: pid="<< i.pid <<" user="<< i.user <<" exe='"<< i.exe
      <<"'\n cwd='"<< i.cwd <<"' root='"<< i.root <<"'\n args=[";

  foreach(const String &arg, i.args)
    out <<"'"<< arg <<"', ";

  out <<"]}";
  return out;
}

std::ostream &operator<<(std::ostream &out, const ProcStat &s) {
  out <<"{ProcStat: cpu_user="<< s.cpu_user <<" cpu_sys="<< s.cpu_sys
      <<" cpu_total="<< s.cpu_total <<" cpu_pct="<< s.cpu_pct
      <<"\n vm_size="<< s.vm_size <<" vm_resident="<< s.vm_resident
      <<" vm_share="<< s.vm_share <<"\n major_faults="<< s.major_faults
      <<" minor_faults="<< s.minor_faults <<" page_faults="<< s.page_faults
      <<'}';
  return out;
}

std::ostream &operator<<(std::ostream &out, const FsStat &s) {
  out <<"{FsStat: prefix='"<< s.prefix <<"' total="<< s.total
      <<" free="<< s.free <<" used="<< s.used <<"\n avail="<< s.avail
      <<" use_pct="<< s.use_pct <<'}';
  return out;
}

std::ostream &operator<<(std::ostream &out, const TermInfo &i) {
  out <<"{TermInfo: term='"<< i.term <<"' num_lines="<< i.num_lines
      <<" num_cols="<< i.num_cols <<'}';
  return out;
}

} // namespace Hypertable
