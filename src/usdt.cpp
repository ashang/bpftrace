#include "usdt.h"

#include <signal.h>

#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include <bcc/bcc_elf.h>
#include <bcc/bcc_usdt.h>

static std::unordered_set<std::string> path_cache;
static std::unordered_set<int> pid_cache;

// Maps all providers of pid to vector of tracepoints on that provider
static std::unordered_map<std::string, usdt_probe_list> usdt_provider_cache;

static void usdt_probe_each(struct bcc_usdt *usdt_probe)
{
  usdt_provider_cache[usdt_probe->provider].emplace_back(usdt_probe_entry{
      .path = usdt_probe->bin_path,
      .provider = usdt_probe->provider,
      .name = usdt_probe->name,
      .num_locations = usdt_probe->num_locations,
  });
}

std::optional<usdt_probe_entry> USDTHelper::find(int pid,
                                                 const std::string &target,
                                                 const std::string &provider,
                                                 const std::string &name)
{
  if (pid > 0)
    read_probes_for_pid(pid);
  else
    read_probes_for_path(target);

  usdt_probe_list probes = usdt_provider_cache[provider];

  auto it = std::find_if(probes.begin(),
                         probes.end(),
                         [&name](const usdt_probe_entry &e) {
                           return e.name == name;
                         });
  if (it != probes.end())
  {
    return *it;
  }
  else
  {
    return std::nullopt;
  }
}

usdt_probe_list USDTHelper::probes_for_pid(int pid)
{
  read_probes_for_pid(pid);

  usdt_probe_list probes;
  for (auto const &usdt_probes : usdt_provider_cache)
  {
    probes.insert(probes.end(),
                  usdt_probes.second.begin(),
                  usdt_probes.second.end());
  }
  return probes;
}

usdt_probe_list USDTHelper::probes_for_path(const std::string &path)
{
  read_probes_for_path(path);

  usdt_probe_list probes;
  for (auto const &usdt_probes : usdt_provider_cache)
  {
    probes.insert(probes.end(),
                  usdt_probes.second.begin(),
                  usdt_probes.second.end());
  }
  return probes;
}

void USDTHelper::read_probes_for_pid(int pid)
{
  if (pid_cache.count(pid))
    return;

  if (pid > 0)
  {
    void *ctx = bcc_usdt_new_frompid(pid, nullptr);
    if (ctx == nullptr)
    {
      std::cerr << "failed to initialize usdt context for pid: " << pid
                << std::endl;
      if (kill(pid, 0) == -1 && errno == ESRCH)
      {
        std::cerr << "hint: process not running" << std::endl;
      }
      return;
    }
    bcc_usdt_foreach(ctx, usdt_probe_each);
    bcc_usdt_close(ctx);

    pid_cache.emplace(pid);
  }
  else
  {
    std::cerr << "a pid must be specified to list USDT probes by PID"
              << std::endl;
  }
}

void USDTHelper::read_probes_for_path(const std::string &path)
{
  if (path_cache.count(path))
    return;

  void *ctx = bcc_usdt_new_frompath(path.c_str());
  if (ctx == nullptr)
  {
    std::cerr << "failed to initialize usdt context for path " << path
              << std::endl;
    return;
  }
  bcc_usdt_foreach(ctx, usdt_probe_each);
  bcc_usdt_close(ctx);

  path_cache.emplace(path);
}
