#pragma once

#include "objfactory.h"
#include "thread_pool.h"

namespace raft
{
    struct Log
    {
        int index = -1;         // 日志记录的索引
        int term = 0;           // 日志记录的任期
        bool is_server = false; // 是否是服务器自己的日志
        std::string content;    // 内容
    };

    enum class State
    {
        None = 0,
        Leader = 1,    // 领导
        Candidate = 2, // 候选人
        Folower = 3,   // 跟随者
    };

    class server : public noncopyable
    {
    private:
        std::mutex m_mutex;
        std::shared_ptr<objfactory<server>> m_factory;

        int m_id = 0;          // server_id
        bool m_is_stop = true; // 停服
        int m_heartbeat = 0;   // 心跳超时计数
        int m_vote_count = 0;  // 拥有的投票数

        // 需要持久化的数据
        State m_state = State::None; // 状态
        int m_term = 0;              // 任期
        int m_votedfor = 0;          // 给谁投票
        std::vector<Log> m_log_vec;  // 日志

        // 临时数据
        int m_commit_index = 0; // 自己的提交进度索引
        int m_last_applied = 0; // 自己的保存进度索引

        // 只属于leader的临时数据
        std::vector<int> m_next_index_vec;  // 所有serve将要同步的进度索引
        std::vector<int> m_match_index_vec; // 所有server已经同步的进度索引

    public:
        server() = delete;
        server(int id, std::shared_ptr<objfactory<server>> factory);
        ~server() = default;

        int key() const { return m_id; }
        bool IsLeader() const { return m_state == State::Leader; }
        int Term() const { return m_term; }
        State GetState() const { return m_state; }
        bool IsStop() const { return m_is_stop; }
        const std::vector<Log> &LogVec() const { return m_log_vec; }
        const std::vector<Log> ApplyLogVec();

        int AddLog(const std::string &str); // 添加日志

        void Start();
        void Stop();
        void ReStart();

    private:
        void Update();   // 定时器
        void Election(); // 选举

        // 请求投票
        struct VoteArgs // 参数
        {
            int term = 0;            // 候选人的任期
            int candidate_id = 0;    // 候选人的id
            int last_log_index = -1; // 候选人最新log的index
            int last_log_term = 0;   // 候选人最新log的任期
        };
        struct VoteReply
        {
            int term = 0;              // 返回的任期
            bool vote_granted = false; // 是否投票
        };
        void RequestVote(const VoteArgs &args);
        void ReplyVote(const VoteReply &reply);

        // 追加条目（可作心跳）
        struct AppendEntriesArgs
        {
            int term = 0;             // 领导的任期
            int leader_id = 0;        // 领导的id
            int pre_log_index = 0;    // 跟随者的同步进度索引
            int pre_log_term = 0;     // 跟随者的同步进度任期
            int commit_index = 0;     // 领导的最新提交索引
            std::vector<Log> log_vec; // 要同步的日志
        };
        struct AppendEntriesReply
        {
            int id = 0;           // 返回的id
            int term = 0;         // 返回的任期
            int log_count = 0;    // 要同步的日志数量
            bool success = false; // 是否同步成功
            int commit_index = 0; // 返回的最新提交索引
        };
        void RequestAppendEntries(const AppendEntriesArgs &args);
        void ReplyAppendEntries(const AppendEntriesReply &reply);

        void ToLeader();
        void ToFollower(int term, int votedfor);

        // Print
    private:
        std::map<long long, std::queue<std::string>> m_print_map;

    public:
        void Print();
        void AddPrint(const std::string &str);
        void PrintAllLog();
        void PrintAllApplyLog();
    };

}
