#include <faabric/planner/PlannerApi.h>
#include <faabric/planner/PlannerClient.h>
#include <faabric/planner/planner.pb.h>
#include <faabric/transport/common.h>
#include <faabric/util/config.h>
#include <faabric/util/locks.h>
#include <faabric/util/logging.h>
#include <faabric/util/network.h>

namespace faabric::planner {
void KeepAliveThread::doWork()
{
    PlannerClient cli;

    faabric::util::SharedLock lock(keepAliveThreadMx);

    cli.registerHost(thisHostReq);
}

void KeepAliveThread::setRequest(
  std::shared_ptr<RegisterHostRequest> thisHostReqIn)
{
    faabric::util::FullLock lock(keepAliveThreadMx);

    thisHostReq = thisHostReqIn;

    // Keep-alive requests should never overwrite the state of the planner
    thisHostReq->set_overwrite(false);
}

PlannerClient::PlannerClient()
  : faabric::transport::MessageEndpointClient(
      faabric::util::getIPFromHostname(
        faabric::util::getSystemConfig().plannerHost),
      PLANNER_ASYNC_PORT,
      PLANNER_SYNC_PORT)
{}

void PlannerClient::ping()
{
    EmptyRequest req;
    PingResponse resp;
    syncSend(PlannerCalls::Ping, &req, &resp);

    // Sanity check
    auto expectedIp = faabric::util::getIPFromHostname(
      faabric::util::getSystemConfig().plannerHost);
    auto gotIp = resp.config().ip();
    // In a k8s deployment, where the planner pod is deployed behind a service,
    // expectedIp will correspond to the service IP, and gotIp to the pod's
    // one (i.e. as reported by the planner locally getting its endpoint host)
    // I don't consider it an error that they differ, and the worker should
    // use the service one
    if (expectedIp != gotIp) {
        SPDLOG_DEBUG(
          "Got two IPs pinging the planner server (our ip: {} - their {})",
          expectedIp,
          gotIp);
    }

    SPDLOG_DEBUG("Succesfully pinged the planner server at {}", expectedIp);
}

std::vector<faabric::planner::Host> PlannerClient::getAvailableHosts()
{
    EmptyRequest req;
    AvailableHostsResponse resp;
    syncSend(PlannerCalls::GetAvailableHosts, &req, &resp);

    // Copy the repeated array into a more convinient container
    std::vector<Host> availableHosts;
    for (int i = 0; i < resp.hosts_size(); i++) {
        availableHosts.push_back(resp.hosts(i));
    }

    return availableHosts;
}

int PlannerClient::registerHost(std::shared_ptr<RegisterHostRequest> req)
{
    RegisterHostResponse resp;
    syncSend(PlannerCalls::RegisterHost, req.get(), &resp);

    if (resp.status().status() != ResponseStatus_Status_OK) {
        throw std::runtime_error("Error registering host with planner!");
    }

    // Sanity check
    assert(resp.config().hosttimeout() > 0);

    // Return the planner's timeout to set the keep-alive heartbeat
    return resp.config().hosttimeout();
}

void PlannerClient::removeHost(std::shared_ptr<RemoveHostRequest> req)
{
    faabric::EmptyResponse response;
    syncSend(PlannerCalls::RemoveHost, req.get(), &response);
}
}
