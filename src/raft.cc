#include "raft.h"

#include <assert.h>
#include <random>
#include <sstream>
#include <algorithm>

#define PRINT(...) PrintOutput(m_factory, m_id, __func__, ##__VA_ARGS__)
template <typename T>
void osspack(std::ostream &o, T &&t) { o << t; }
template <typename T, typename... Args>
void osspack(std::ostream &o, T &&t, Args &&...args)
{
    o << t;
    osspack(o, std::forward<Args>(args)...);
}
template <typename... Args>
void PrintOutput(std::shared_ptr<raft::objfactory<raft::server>> factory, int id, const std::string &func, Args &&...args)
{
    auto server = factory->Get(id, factory);
    std::ostringstream o;
    osspack(o, "(", id, " ", server->Term(), " ", (int)server->GetState(), ") ", func, "->", std::forward<Args>(args)...);
    factory->Get(0, factory)->AddPrint(o.str());
}

raft::server::server(int id, std::shared_ptr<objfactory<server>> factory)
{
    assert(id >= 0);
    assert(factory);
    m_id = id;
    m_factory = factory;
}

const std::vector<raft::Log> raft::server::ApplyLogVec()
{
    std::unique_lock<std::mutex> _(m_mutex);
    std::vector<Log> log_vec;
    for (int i = 0; i <= m_last_applied && i < (int)m_log_vec.size(); ++i)
    {
        if (!m_log_vec[i].is_server)
            log_vec.push_back(m_log_vec[i]);
    }
    return log_vec;
}

int raft::server::AddLog(const std::string &str)
{
    std::unique_lock<std::mutex> _(m_mutex);
    if (m_is_stop || m_state != State::Leader)
        return m_votedfor;

    const auto &index = (int)m_log_vec.size();
    m_log_vec.push_back(Log{index, m_term, false, str});
    PRINT("index:", index, " term:", m_term, " content:", str);
    return 0;
}

void raft::server::Start()
{
    std::unique_lock<std::mutex> _(m_mutex);
    m_is_stop = false;
    m_heartbeat = 0;
    m_vote_count = 0;

    m_state = State::Folower;
    m_term = 0;
    m_votedfor = 0;
    m_log_vec.clear();
    m_log_vec.push_back(Log{(int)m_log_vec.size(), 0, true, "Start"}); // 初始化一条日志

    m_commit_index = 0;
    m_last_applied = 0;

    m_next_index_vec.clear();
    m_match_index_vec.clear();

    //启动定时器
    auto tmp = m_factory->Get(m_id, m_factory);
    thread_pool::get(0).submit([tmp]
                               { tmp->Update(); });
}

void raft::server::Stop()
{
    std::unique_lock<std::mutex> _(m_mutex);
    m_is_stop = true;
    PRINT("");
}

void raft::server::ReStart()
{
    std::unique_lock<std::mutex> _(m_mutex);
    m_is_stop = false;
    m_heartbeat = 0;
    m_vote_count = 0;

    // m_state = State::Folower;
    // m_term = 0;
    // m_votedfor = 0;
    // m_log_vec.clear();
    // m_log_vec.push_back(Log{(int)m_log_vec.size(), 0, true, "Start"}); // 初始化一条日志

    // m_commit_index = 0;
    // m_last_applied = 0;

    m_next_index_vec.clear();
    m_match_index_vec.clear();
    PRINT("");
}

void raft::server::Update()
{
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        std::unique_lock<std::mutex> _(m_mutex);
        if (m_is_stop)
            continue;

        switch (m_state)
        {
        case State::Leader:
        {
            // 领导同步日志信息，发0条当心跳
            AppendEntriesArgs args;
            args.term = m_term;
            args.leader_id = m_id;
            args.commit_index = m_commit_index;
            for (const auto &id : m_factory->GetAllObjKey())
            {
                if (id == m_id || id < 0 || id >= m_next_index_vec.size())
                    continue;

                const auto &next_index = m_next_index_vec[id];
                args.pre_log_index = next_index - 1;
                args.pre_log_term = args.pre_log_index >= 0 && args.pre_log_index < (int)m_log_vec.size() ? m_log_vec[args.pre_log_index].term : 0;

                args.log_vec.clear();
                for (int i = next_index; i < (int)m_log_vec.size(); ++i)
                    args.log_vec.push_back(m_log_vec[i]);

                auto tmp = m_factory->Get(id, m_factory);
                thread_pool::get(0).submit([tmp, args]
                                           { tmp->RequestAppendEntries(args); });
            }
        }
        break;
        case State::Candidate:
            break;
        case State::Folower:
        {
            // 心跳计数，超时则触发选举
            if (++m_heartbeat == 6)
            {
                m_heartbeat = 0;
                m_state = State::Candidate;
                auto tmp = m_factory->Get(m_id, m_factory);
                thread_pool::get(0).submit([tmp]
                                           { tmp->Election(); });
            }
        }
        break;
        default:
            return;
        }

        // 保存日志
        if (m_state == State::Leader && !m_match_index_vec.empty())
        {
            // 不负责为之前的领导留下的过半复制日志专门进行提交，只提交自己任期内的日志
            auto match_vec = m_match_index_vec;
            std::sort(match_vec.begin(), match_vec.end());
            const auto &mid_index = match_vec[(int)match_vec.size() / 2]; // 超过半数提交
            if (m_log_vec[mid_index].term == m_term)
            {
                // 提交自己任期日志时能够自动把之前的都提交
                m_commit_index = std::max(mid_index, m_commit_index);
            }
        }

        for (; m_last_applied <= m_commit_index; ++m_last_applied)
        {
            if (m_last_applied >= 0 && m_last_applied < (int)m_log_vec.size())
            {
                const auto &log = m_log_vec[m_last_applied];
                if (!log.is_server)
                    PRINT("apply_log[", m_last_applied, "]{index:", log.index, " term:", log.term, " content:", log.content, "}");
            }
            else
            {
                PRINT("apply_log[", m_last_applied, "] fail");
            }
        }
    }
}

void raft::server::Election()
{
    while (true)
    {
        // 随机计时器
        std::random_device rd;
        std::default_random_engine eng(rd());
        std::uniform_int_distribution<int> dist(100, 300);
        const auto &sleep = dist(eng);
        PRINT("sleep:", sleep);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep));

        std::unique_lock<std::mutex> _(m_mutex);
        if (m_is_stop || m_state != State::Candidate) // 计时期间可能不是候选人了
            break;

        // 任期+1，并投自己一票
        m_vote_count = 1;
        ++m_term;
        m_votedfor = m_id;
        PRINT("vote self");

        // 发起请求投票
        const VoteArgs &args{m_term, m_id, (int)m_log_vec.size() - 1, m_log_vec.empty() ? 0 : m_log_vec.back().term};
        for (const auto &id : m_factory->GetAllObjKey())
        {
            if (id == m_id)
                continue;

            auto tmp = m_factory->Get(id, m_factory);
            thread_pool::get(0).submit([tmp, args]
                                       { tmp->RequestVote(args); });
        }
    }
}

void raft::server::RequestVote(const VoteArgs &args)
{
    std::unique_lock<std::mutex> _(m_mutex);
    if (m_is_stop)
        return;

    VoteReply reply{};

    // 候选人任期比我大，或者我没有投票，或者投的是同一个候选人
    if (args.term >= m_term || m_votedfor == 0 || m_votedfor == args.candidate_id)
    {
        // 候选人的日志至少要和我一样新
        const auto &last_log_term = m_log_vec.empty() ? 0 : m_log_vec.back().term;
        if (args.last_log_term > last_log_term ||
            (args.last_log_term == last_log_term && args.last_log_index >= (int)m_log_vec.size() - 1))
        {
            ToFollower(args.term, args.candidate_id);
            reply.vote_granted = true;
        }
    }

    if (!reply.vote_granted && m_state == State::Leader)
        ToFollower(args.term, 0);

    reply.term = m_term;

    PRINT(reply.vote_granted ? "vote " : "not_vote ", args.candidate_id);

    // 投票返回
    auto tmp = m_factory->Get(args.candidate_id, m_factory);
    thread_pool::get(0).submit([tmp, reply]
                               { tmp->ReplyVote(reply); });
}

void raft::server::ReplyVote(const VoteReply &reply)
{
    std::unique_lock<std::mutex> _(m_mutex);
    if (m_is_stop || m_state != State::Candidate) // 收到投票返回时可能不是候选人了
        return;

    if (reply.vote_granted)
    {
        // 同意，则投票数+1，如果获得超过半数的投票，则当选领导
        if (++m_vote_count * 2 >= (int)m_factory->GetAllObjKey().size() - 1)
            ToLeader();
    }
    else if (reply.term > m_term)
    {
        // 任期比我大
        ToFollower(reply.term, 0);
    }
}

void raft::server::RequestAppendEntries(const AppendEntriesArgs &args)
{
    std::unique_lock<std::mutex> _(m_mutex);
    if (m_is_stop)
        return;

    AppendEntriesReply reply{};

    if (args.term < m_term)
    {
        // 我的任期比领导的大，则无视
        reply.success = false;
        PRINT("term bigger than leader");
    }
    else
    {
        // 重置心跳
        m_heartbeat = 0;

        // 任期要与领导一致
        m_term = args.term;

        if (m_state != State::Folower)
            ToFollower(args.term, args.leader_id);

        if (args.log_vec.empty())
        {
            // 我的提交进度索引比领导的小，说明我进度落后了
            // 领导记录到关于我的提交进度，与我实际进度一致
            // 则更新我的提交进度索引
            if (m_commit_index < args.commit_index &&
                args.pre_log_index >= 0 &&
                args.pre_log_index < (int)m_log_vec.size() &&
                m_log_vec[args.pre_log_index].term == args.pre_log_term)
            {
                m_commit_index = std::min(args.pre_log_index, args.commit_index);
            }

            // 心跳无返回
            return;
        }

        if (m_commit_index >= args.pre_log_index + (int)args.log_vec.size())
        {
            // 发过来的日志都在我的提交进度内，返回成功
            reply.success = true;
        }
        else if ((args.pre_log_index >= (int)m_log_vec.size()) ||
                 (args.pre_log_index >= 0 && m_log_vec[args.pre_log_index].term != args.pre_log_term))
        {
            if ((args.pre_log_index >= (int)m_log_vec.size()))
                PRINT("not_match ", args.pre_log_index, " >= ", m_log_vec.size());
            else
                PRINT("not_match ", args.pre_log_index, " >= 0 && ", m_log_vec[args.pre_log_index].term, " != ", args.pre_log_term);

            // 领导记录关于我的提交进度，与我实际进度不一致，则返回失败
            reply.success = false;
        }
        else
        {
            PRINT("log_push ", m_commit_index, " ", args.commit_index, " ", args.pre_log_index, " ", args.pre_log_term, " ", args.log_vec.size());

            // 领导记录关于我的提交进度，与我实际进度一致，则添加新日志，返回成功
            m_log_vec.resize(args.pre_log_index + 1);
            m_log_vec.insert(m_log_vec.end(), args.log_vec.begin(), args.log_vec.end());

            if (m_commit_index < args.commit_index)
            {
                // 更新我的提交记录
                m_commit_index = std::min(args.commit_index, std::max(args.pre_log_index, m_commit_index));
            }

            reply.success = true;
        }
    }

    reply.id = m_id;
    reply.term = m_term;
    reply.log_count = (int)args.log_vec.size();
    reply.commit_index = m_commit_index;

    auto tmp = m_factory->Get(args.leader_id, m_factory);
    thread_pool::get(0).submit([tmp, reply]
                               { tmp->ReplyAppendEntries(reply); });
}

void raft::server::ReplyAppendEntries(const AppendEntriesReply &reply)
{
    std::unique_lock<std::mutex> _(m_mutex);
    if (m_is_stop || m_state != State::Leader)
    {
        PRINT("return stop:", m_is_stop, " state:", (int)m_state);
        return;
    }

    if (reply.id < 0 || reply.id >= (int)m_next_index_vec.size())
    {
        PRINT("return id:", reply.id);
        return;
    }

    if (reply.success)
    {
        if (reply.id >= (int)m_match_index_vec.size())
        {
            PRINT("return id2:", reply.id);
            return;
        }

        if (reply.log_count > 0)
            PRINT("succ ", reply.id, " ", m_match_index_vec[reply.id], " ", m_next_index_vec[reply.id], " count:", reply.log_count);

        // 添加成功，更新跟随者的提交进度和同步进度
        m_match_index_vec[reply.id] = std::max(m_match_index_vec[reply.id], m_next_index_vec[reply.id] + reply.log_count - 1);
        m_next_index_vec[reply.id] += reply.log_count;
    }
    else
    {
        if (reply.term <= m_term)
        {
            // 添加失败，更新跟随者的提交进度
            PRINT("fail ", reply.id, " ", m_next_index_vec[reply.id], " ", reply.commit_index);
            m_next_index_vec[reply.id] = reply.commit_index + 1;
        }
        else
        {
            // 任期比我大，不处理
        }
    }
}

void raft::server::ToLeader()
{
    m_state = State::Leader;
    m_heartbeat = 0;
    m_vote_count = 0;
    m_votedfor = 0;

    {
        const auto &len = m_factory->GetAllObjKey().size();
        m_next_index_vec.clear();
        m_next_index_vec.resize(len, 0);
        m_match_index_vec.clear();
        m_match_index_vec.resize(len, 0);
    }

    m_log_vec.push_back(Log{(int)m_log_vec.size(), m_term, true, "ToLeader:" + std::to_string(m_id)});
    PRINT("");
}

void raft::server::ToFollower(int term, int votedfor)
{
    m_state = State::Folower;
    m_heartbeat = 0;
    m_vote_count = 0;
    m_term = term;
    m_votedfor = votedfor;
    PRINT("");
}

void raft::server::Print()
{
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        std::unique_lock<std::mutex> _(m_mutex);
        if (m_print_map.empty())
            continue;

        auto it = m_print_map.begin();
        while (!it->second.empty())
        {
            printf("[%lld] %s\n", it->first, it->second.front().c_str());
            it->second.pop();
        }
        m_print_map.erase(it);
    }
}

void raft::server::AddPrint(const std::string &str)
{
    std::unique_lock<std::mutex> _(m_mutex);
    m_print_map[std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock().now().time_since_epoch()).count()].push(str);
}

void raft::server::PrintAllLog()
{
    std::unique_lock<std::mutex> _(m_mutex);
    for (const auto &log : m_log_vec)
        PRINT("index:", log.index, " term:", log.term, " is_server:", log.is_server, " content:", log.content);
}

void raft::server::PrintAllApplyLog()
{
    const auto &log_vec = ApplyLogVec();
    std::unique_lock<std::mutex> _(m_mutex);
    for (const auto &log : log_vec)
        PRINT("index:", log.index, " term:", log.term, " is_server:", log.is_server, " content:", log.content);
}
