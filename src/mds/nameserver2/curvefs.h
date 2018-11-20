/*
 * Project: curve
 * Created Date: Friday September 7th 2018
 * Author: hzsunjianliang
 * Copyright (c) 2018 netease
 */

#ifndef SRC_MDS_NAMESERVER2_CURVEFS_H_
#define SRC_MDS_NAMESERVER2_CURVEFS_H_

#include <vector>
#include <string>
#include <thread>  //NOLINT
#include "proto/nameserver2.pb.h"
#include "src/mds/nameserver2/namespace_storage.h"
#include "src/mds/nameserver2/inode_id_generator.h"
#include "src/mds/nameserver2/define.h"
#include "src/mds/nameserver2/chunk_allocator.h"
#include "src/mds/nameserver2/clean_manager.h"
#include "src/mds/nameserver2/async_delete_snapshot_entity.h"
#include "src/mds/nameserver2/session.h"


namespace curve {
namespace mds {


const uint64_t ROOTINODEID = 0;
const char ROOTFILENAME[] = "/";
const uint64_t INTERALGARBAGEDIR = 1;

using ::curve::mds::DeleteSnapShotResponse;

class CurveFS {
 public:
    // singleton, supported in c++11
    static CurveFS &GetInstance() {
        static CurveFS curvefs;
        return curvefs;
    }

    /**
     *  @brief CurveFS初始化
     *  @param NameServerStorage:
     *         InodeIDGenerator：
     *         ChunkSegmentAllocator：
     *         CleanManagerInterface:
     *         sessionManager：
     *         sessionOptions ：初始化所session需要的参数
     *  @return 初始化是否成功
     */
    bool Init(NameServerStorage*, InodeIDGenerator*, ChunkSegmentAllocator*,
              std::shared_ptr<CleanManagerInterface>,
              SessionManager *sessionManager,
              const struct SessionOptions &sessionOptions);

    /**
     *  @brief CurveFS Uninit
     *  @param
     *  @return
     */
    void Uninit();

    // namespace ops
    /**
     *  @brief 创建文件
     *  @param fileName: 文件名
     *         filetype：文件类型
     *         length：文件长度
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    StatusCode CreateFile(const std::string & fileName,
                          FileType filetype,
                          uint64_t length);
    /**
     *  @brief 获取文件信息
     *  @param filename：文件名
     *         inode：返回获取到的文件系统
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    StatusCode GetFileInfo(const std::string & filename,
                           FileInfo * inode) const;

    /**
     *  @brief 删除文件
     *  @param filename：文件名
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    StatusCode DeleteFile(const std::string & filename);

    /**
     *  @brief 获取目录下所有文件信息
     *  @param dirname：目录名字
     *         files：返回查到的结果
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    StatusCode ReadDir(const std::string & dirname,
                       std::vector<FileInfo> * files) const;

    /**
     *  @brief 重命名文件
     *  @param oldFileName：旧文件名
     *         newFileName：希望重命名的新文件名
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    StatusCode RenameFile(const std::string & oldFileName,
                          const std::string & newFileName);

    /**
     *  @brief 扩容文件
     *  @param filename：文件名
     *         newSize：希望扩容后的文件大小
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    // extent size minimum unit 1GB ( segement as a unit)
    StatusCode ExtendFile(const std::string &filename,
                          uint64_t newSize);


    // segment(chunk) ops
    /**
     *  @brief 查询segment信息，如果segment不存在，根据allocateIfNoExist决定是否
     *         创建新的segment
     *  @param filename：文件名
     *         offset: segment的偏移
     *         allocateIfNoExist：如果segment不存在，是否需要创建新的segment
     *         segment：返回查询到的segment信息
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    StatusCode GetOrAllocateSegment(
        const std::string & filename,
        offset_t offset,
        bool allocateIfNoExist, PageFileSegment *segment);

    /**
     *  @brief 删除segment
     *  @param filename：文件名
     *         offset: segment的偏移
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    StatusCode DeleteSegment(const std::string &filename, offset_t offset);

    /**
     *  @brief 获取root文件信息
     *  @param
     *  @return 返回获取到的root文件信息
     */
    FileInfo GetRootFileInfo(void) const {
        return rootFileInfo_;
    }

    StatusCode CreateSnapShotFile(const std::string &fileName,
                            FileInfo *snapshotFileInfo);
    StatusCode ListSnapShotFile(const std::string & fileName,
                            std::vector<FileInfo> *snapshotFileInfos) const;
    // async interface
    StatusCode DeleteFileSnapShotFile(const std::string &fileName,
                            FileSeqType seq,
                            std::shared_ptr<AsyncDeleteSnapShotEntity> entity);
    StatusCode CheckSnapShotFileStatus(const std::string &fileName,
                            FileSeqType seq, FileStatus * status,
                            uint32_t * progress) const;

    StatusCode GetSnapShotFileInfo(const std::string &fileName,
                            FileSeqType seq, FileInfo *snapshotFileInfo) const;

    StatusCode GetSnapShotFileSegment(
            const std::string & filename,
            FileSeqType seq,
            offset_t offset,
            PageFileSegment *segment);

    // session ops
    /**
     *  @brief 打开文件
     *  @param filename：文件名
     *         clientIP：clientIP
     *         session：返回创建的session信息
     *         fileInfo：返回打开的文件信息
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    StatusCode OpenFile(const std::string &fileName,
                        const std::string &clientIP,
                        ProtoSession *protoSession,
                        FileInfo  *fileInfo);

    /**
     *  @brief 关闭文件
     *  @param fileName: 文件名
     *         sessionID：sessionID
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    StatusCode CloseFile(const std::string &fileName,
                         const std::string &sessionID);

    /**
     *  @brief 更新session的有效期
     *  @param filename：文件名
     *         sessionid：sessionID
     *         date: 请求的时间，用来防止重放攻击
     *         signature: 用来进行请求的身份验证
     *         clientIP: clientIP
     *         fileInfo: 返回打开的文件信息
     *  @return 是否成功，成功返回StatusCode::kOK
     */
    StatusCode RefreshSession(const std::string &filename,
                              const std::string &sessionid,
                              const uint64_t date,
                              const std::string &signature,
                              const std::string &clientIP,
                              FileInfo  *fileInfo);

    // TODO(hzsunjianliang): snapshot ops
 private:
    CurveFS() = default;

    void InitRootFile(void);

    StatusCode WalkPath(const std::string &fileName,
                        FileInfo *fileInfo, std::string  *lastEntry) const;

    StatusCode LookUpFile(const FileInfo & parentFileInfo,
                          const std::string & fileName,
                          FileInfo *fileInfo) const;

    StatusCode PutFile(const FileInfo & fileInfo);

    /**
     * @brief 执行一次fileinfo的snapshot快照事务
     * @param originalFileInfo: 原文件对于fileInfo
     * @param SnapShotFile: 生成的snapshot文件对于fileInfo
     * @return StatusCode: 成功或者失败
     */
    StatusCode SnapShotFile(const FileInfo * originalFileInfo,
        const FileInfo * SnapShotFile) const;

 private:
    FileInfo rootFileInfo_;
    NameServerStorage*          storage_;
    InodeIDGenerator*           InodeIDGenerator_;
    ChunkSegmentAllocator*      chunkSegAllocator_;
    SessionManager *            sessionManager_;
    std::shared_ptr<CleanManagerInterface> snapshotCleanManager_;
};
extern CurveFS &kCurveFS;
}   // namespace mds
}   // namespace curve
#endif   // SRC_MDS_NAMESERVER2_CURVEFS_H_
