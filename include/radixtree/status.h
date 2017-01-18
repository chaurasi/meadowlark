/*
 * This class is structured similar to Googles grpc status class.
 * It is intended to provide a simple and efficient way to return
 * an error code with a message.
 *
 * The OK error code is not intended to have a return message in
 * order to keep overhead low.
 *
 *
 * Example use:
 *
 * #include "include/status.h"
 *
 * Status foo() {
 *   ...
 *   if (error) {
 *     return Status(StatusCode::NOT_VALID, "Detailed message");
 *   }
 *   return Status::OK;
 * }
 *
 * ...
 *
 * Status res = foo();
 *
 * if (res.notOk()) {
 *   LOG(error) << "Error result from foo: " << res.error_message();
 * }
 *
 *
 */

#ifndef STATUS_H
#define STATUS_H

#include <string>


namespace radixtree {

enum StatusCode {
  /// General error codes. We should try to use these as much as possible

  /// Not an error; returned on success.
  OK = 0,

  /// The operation was cancelled (typically by the caller).
  CANCELLED = 1,

  /// Unknown error. An example of where this error may be returned is if a
  /// Status value received from another address space belongs to an error-space
  /// that is not known in this address space. Also errors raised by APIs that
  /// do not return enough error information may be converted to this error.
  UNKNOWN = 2,

  /// Client specified an invalid argument. Note that this differs from
  /// FAILED_PRECONDITION. INVALID_ARGUMENT indicates arguments that are
  /// problematic regardless of the state of the system (e.g., a malformed file
  /// name).
  INVALID_ARGUMENT = 3,

  /// Deadline expired before operation could complete. For operations that
  /// change the state of the system, this error may be returned even if the
  /// operation has completed successfully. For example, a successful response
  /// from a server could have been delayed long enough for the deadline to
  /// expire.
  DEADLINE_EXCEEDED = 4,

  /// Some requested entity (e.g., file or directory) was not found.
  NOT_FOUND = 5,

  /// Some entity that we attempted to create (e.g., file or directory) already
  /// exists.
  ALREADY_EXISTS = 6,

  /// The caller does not have permission to execute the specified operation.
  /// PERMISSION_DENIED must not be used for rejections caused by exhausting
  /// some resource (use RESOURCE_EXHAUSTED instead for those errors).
  /// PERMISSION_DENIED must not be used if the caller can not be identified
  /// (use UNAUTHENTICATED instead for those errors).
  PERMISSION_DENIED = 7,

  /// The request does not have valid authentication credentials for the
  /// operation.
  UNAUTHENTICATED = 16,

  /// Some resource has been exhausted, perhaps a per-user quota, or perhaps the
  /// entire file system is out of space.
  RESOURCE_EXHAUSTED = 8,

  /// Operation was rejected because the system is not in a state required for
  /// the operation's execution. For example, directory to be deleted may be
  /// non-empty, an rmdir operation is applied to a non-directory, etc.
  ///
  /// A litmus test that may help a service implementor in deciding
  /// between FAILED_PRECONDITION, ABORTED, and UNAVAILABLE:
  ///  (a) Use UNAVAILABLE if the client can retry just the failing call.
  ///  (b) Use ABORTED if the client should retry at a higher-level
  ///      (e.g., restarting a read-modify-write sequence).
  ///  (c) Use FAILED_PRECONDITION if the client should not retry until
  ///      the system state has been explicitly fixed. E.g., if an "rmdir"
  ///      fails because the directory is non-empty, FAILED_PRECONDITION
  ///      should be returned since the client should not retry unless
  ///      they have first fixed up the directory by deleting files from it.
  ///  (d) Use FAILED_PRECONDITION if the client performs conditional
  ///      REST Get/Update/Delete on a resource and the resource on the
  ///      server does not match the condition. E.g., conflicting
  ///      read-modify-write on the same resource.
  FAILED_PRECONDITION = 9,

  /// The operation was aborted, typically due to a concurrency issue like
  /// sequencer check failures, transaction aborts, etc.
  ///
  /// See litmus test above for deciding between FAILED_PRECONDITION, ABORTED,
  /// and UNAVAILABLE.
  ABORTED = 10,

  /// Operation was attempted past the valid range. E.g., seeking or reading
  /// past end of file.
  ///
  /// Unlike INVALID_ARGUMENT, this error indicates a problem that may be fixed
  /// if the system state changes. For example, a 32-bit file system will
  /// generate INVALID_ARGUMENT if asked to read at an offset that is not in the
  /// range [0,2^32-1], but it will generate OUT_OF_RANGE if asked to read from
  /// an offset past the current file size.
  ///
  /// There is a fair bit of overlap between FAILED_PRECONDITION and
  /// OUT_OF_RANGE. We recommend using OUT_OF_RANGE (the more specific error)
  /// when it applies so that callers who are iterating through a space can
  /// easily look for an OUT_OF_RANGE error to detect when they are done.
  OUT_OF_RANGE = 11,

  /// Operation is not implemented or not supported/enabled in this service.
  UNIMPLEMENTED = 12,

  /// Internal errors. Means some invariants expected by underlying System has
  /// been broken. If you see one of these errors, Something is very broken.
  INTERNAL = 13,

  /// The service is currently unavailable. This is a most likely a transient
  /// condition and may be corrected by retrying with a backoff.
  ///
  /// See litmus test above for deciding between FAILED_PRECONDITION, ABORTED,
  /// and UNAVAILABLE.
  UNAVAILABLE = 14,

  /// Unrecoverable data loss or corruption.
  DATA_LOSS = 15,

  /// Operation failed
  FAILED = 16,


  /// Object is not initialized and ready for use
  NOT_INITIALIZED = 17,


  /// A required object is not valid
  NOT_VALID = 18,

  /// Not an error condition, it indicates we have reached end of all available data
  END_OF_DATA = 19,
  

  /// If required we can add additional error codes here. Please do not add redundant
  /// ERROR codes, such as ITEM_STORE_FAILED for each module. Instead use FAILED.
  /// The caller typically knows which module and function was called.




  /// Force users to include a default branch:
  DO_NOT_USE = -1
};


class Status {
 public:
  /// Construct an OK instance.
    Status() : code_(StatusCode::OK) {}

  /// Construct an instance with associated \a code and \a details (also
  // referred to as "error_message").
    Status(StatusCode code, const std::string& details)
      : code_(code), details_(details) {}

  /// A pre-defined OK instance.
  static const Status& OK;


  /// Return the instance's error code.
  StatusCode error_code() const { return code_; }
  /// Return the instance's error message.
  std::string error_message() const { return details_; }

  /// Is the status OK?
  bool ok() const { return code_ == StatusCode::OK; }

  bool notOk() const { return code_ != StatusCode::OK; }

 private:
  StatusCode code_;
  std::string details_;
};

}  

#endif 