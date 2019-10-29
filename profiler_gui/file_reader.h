//
// Created by vzarubkin on 29.10.19.
//

#ifndef EASY_PROFILER_FILE_READER_H
#define EASY_PROFILER_FILE_READER_H

#include <atomic>
#include <sstream>
#include <thread>

#include <QObject>
#include <QString>

#include <easy/reader.h>

EASY_CONSTEXPR auto NETWORK_CACHE_FILE = "easy_profiler_stream.cache";

class FileReader Q_DECL_FINAL
{
    enum class JobType : int8_t
    {
        Idle = 0,
        Loading,
        Saving,
    };

    profiler::SerializedData      m_serializedBlocks; ///<
    profiler::SerializedData m_serializedDescriptors; ///<
    profiler::descriptors_list_t       m_descriptors; ///<
    profiler::blocks_t                      m_blocks; ///<
    profiler::thread_blocks_tree_t      m_blocksTree; ///<
    profiler::bookmarks_t                m_bookmarks; ///<
    profiler::BeginEndTime            m_beginEndTime; ///<
    std::stringstream                       m_stream; ///<
    std::stringstream                 m_errorMessage; ///<
    QString                               m_filename; ///<
    profiler::processid_t                  m_pid = 0; ///<
    uint32_t           m_descriptorsNumberInFile = 0; ///<
    uint32_t                           m_version = 0; ///<
    std::thread                             m_thread; ///<
    std::atomic_bool                         m_bDone; ///<
    std::atomic<int>                      m_progress; ///<
    std::atomic<unsigned int>                 m_size; ///<
    JobType                m_jobType = JobType::Idle; ///<
    bool                            m_isFile = false; ///<
    bool                        m_isSnapshot = false; ///<

public:

    FileReader();
    ~FileReader();

    const bool isFile() const;
    const bool isSaving() const;
    const bool isLoading() const;
    const bool isSnapshot() const;

    bool done() const;
    int progress() const;
    unsigned int size() const;
    const QString& filename() const;

    void load(const QString& _filename);
    void load(std::stringstream& _stream);

    /** \brief Save data to file.
    */
    void save(const QString& _filename, profiler::timestamp_t _beginTime, profiler::timestamp_t _endTime,
              const profiler::SerializedData& _serializedDescriptors, const profiler::descriptors_list_t& _descriptors,
              profiler::block_id_t descriptors_count, const profiler::thread_blocks_tree_t& _trees,
              const profiler::bookmarks_t& bookmarks, profiler::block_getter_fn block_getter,
              profiler::processid_t _pid, bool snapshotMode);

    void interrupt();

    void get(profiler::SerializedData& _serializedBlocks, profiler::SerializedData& _serializedDescriptors,
             profiler::descriptors_list_t& _descriptors, profiler::blocks_t& _blocks, profiler::thread_blocks_tree_t& _trees,
             profiler::bookmarks_t& bookmarks, profiler::BeginEndTime& beginEndTime, uint32_t& _descriptorsNumberInFile,
             uint32_t& _version, profiler::processid_t& _pid, QString& _filename);

    void join();

    QString getError() const;

}; // END of class FileReader.

#endif //EASY_PROFILER_FILE_READER_H
