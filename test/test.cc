#include "raft.h"

#include <assert.h>

int GetLeaderID(std::shared_ptr<raft::objfactory<raft::server>> factory)
{
    int leader_id = 0;
    int term = -1;
    for (const auto &id : factory->GetAllObjKey())
    {
        auto tmp = factory->Get(id, factory);
        if (tmp->IsStop())
            continue;

        // 在同一个任期内，只有一个leader
        if (!tmp->IsLeader())
            continue;
        if (leader_id == 0)
            leader_id = id;
        assert(leader_id == id);
    }
    return leader_id;
}

// 检测日志是否一致
void CheckApplyLog(std::shared_ptr<raft::objfactory<raft::server>> factory)
{
    const auto &leader_id = GetLeaderID(factory);
    assert(leader_id != 0);
    const auto &log_vec = factory->Get(leader_id, factory)->ApplyLogVec();
    for (const auto &id : factory->GetAllObjKey())
    {
        auto tmp = factory->Get(id, factory);
        if (tmp->IsStop())
            continue;

        const auto &tmp_log_vec = tmp->ApplyLogVec();
        assert(log_vec.size() == tmp_log_vec.size());
        for (int i = 0; i < (int)log_vec.size(); ++i)
        {
            assert(log_vec[i].index == tmp_log_vec[i].index);
            assert(log_vec[i].term == tmp_log_vec[i].term);
            assert(log_vec[i].is_server == tmp_log_vec[i].is_server);
            assert(log_vec[i].content == tmp_log_vec[i].content);
        }
    }
}

int main()
{
    // 对象池
    auto factory = std::make_shared<raft::objfactory<raft::server>>();

    // 线程池
    auto &tpool = raft::thread_pool::get(100);

    // 打印线程
    auto print = factory->Get(0, factory);
    tpool.submit([print]
                 { print->Print(); });

    const auto &MAX_SERVER = 5;

    // 服务器启动，选举出一个leader
    print->AddPrint("\n\nTest->ALL server Start, server_count:" + std::to_string(MAX_SERVER));
    for (int i = 1; i <= MAX_SERVER; ++i)
        factory->Get(i, factory)->Start();
    std::this_thread::sleep_for(std::chrono::seconds(15));
    const auto &leader1 = GetLeaderID(factory);
    assert(leader1 != 0);

    // 领导掉线，重新选举
    print->AddPrint("\n\nTest->Server:" + std::to_string(leader1) + " Disconnect");
    factory->Get(leader1, factory)->Stop();
    std::this_thread::sleep_for(std::chrono::seconds(10));
    const auto &leader2 = GetLeaderID(factory);
    assert(leader2 != 0);

    // 追加日志，掉线的服务器无法完成同步的
    print->AddPrint("\n\nTest->Server:" + std::to_string(leader2) + " Add Log");
    factory->Get(leader2, factory)->AddLog("test_1");
    std::this_thread::sleep_for(std::chrono::seconds(10));
    assert(factory->Get(leader1, factory)->ApplyLogVec().size() == 0);
    CheckApplyLog(factory);

    // 掉线的服务器重新上线，同步日志
    print->AddPrint("\n\nTest->Server:" + std::to_string(leader1) + " Connect");
    factory->Get(leader1, factory)->ReStart();
    std::this_thread::sleep_for(std::chrono::seconds(10));
    CheckApplyLog(factory);

    // 超过一半的服务器掉线，无法完成选举
    print->AddPrint("\n\nTest->Leader:" + std::to_string(leader2) + " And More Than Half Server Disconnect");
    factory->Get(leader2, factory)->Stop();
    int disconnect_count = MAX_SERVER / 2;
    for (const auto &id : factory->GetAllObjKey())
    {
        auto tmp = factory->Get(id, factory);
        if (tmp->IsStop())
            continue;
        tmp->Stop();
        if (--disconnect_count <= 0)
            break;
    }
    std::this_thread::sleep_for(std::chrono::seconds(3));
    assert(GetLeaderID(factory) == 0);

    // 上线一台服务器，超过一半的服务器上线，重新选举
    print->AddPrint("\n\nTest->Server:" + std::to_string(leader2) + " Connect");
    factory->Get(leader2, factory)->ReStart();
    std::this_thread::sleep_for(std::chrono::seconds(10));
    const auto &leader3 = GetLeaderID(factory);
    assert(leader3 != 0);

    // 下线一台非领导的服务器
    print->AddPrint("\n\nTest->Disconnect One Server");
    for (const auto &id : factory->GetAllObjKey())
    {
        auto tmp = factory->Get(id, factory);
        if (!tmp->IsStop() && !tmp->IsLeader())
        {
            tmp->Stop();
            break;
        }
    }
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // 超过一半的服务器掉线，无法完成日志同步
    print->AddPrint("\n\nTest->Add Log");
    factory->Get(leader3, factory)->AddLog("test_2");
    factory->Get(leader3, factory)->AddLog("test_3");
    std::this_thread::sleep_for(std::chrono::seconds(10));
    CheckApplyLog(factory);

    // 所有掉线的服务器重新上线，同步日志
    print->AddPrint("\n\nTest->All Server Connect");
    for (const auto &id : factory->GetAllObjKey())
    {
        auto tmp = factory->Get(id, factory);
        if (tmp->IsStop() && id != 0)
            tmp->ReStart();
    }
    std::this_thread::sleep_for(std::chrono::seconds(10));
    CheckApplyLog(factory);

    // 打印所有服务器的日志
    print->AddPrint("\n\nTest->All Server Print Apply Log");
    for (const auto &id : factory->GetAllObjKey())
    {
        auto tmp = factory->Get(id, factory);
        if (!tmp->IsStop() && id != 0)
            tmp->PrintAllApplyLog();
    }

    return 0;
}
