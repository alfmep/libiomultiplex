/*
 * Copyright (C) 2021,2022 Dan Arrhenius <dan@ultramarin.se>
 *
 * This file is part of libiomultiplex
 *
 * libiomultiplex is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef IOMULTIPLEX_LOG_HPP
#define IOMULTIPLEX_LOG_HPP

#include <functional>
#include <atomic>
#include <string>
#include <mutex>
#include <cstdlib>
#include <cstdarg>
#include <syslog.h>


namespace iomultiplex {


    /**
     * Callback for log messages.
     * This callback is called whenever a message is logged with
     * a method in class Log.
     *
     * <b>NOTE:</b> Do not call Log::set_callback from inside
     *              the callback since it will cause a deadlock!
     *
     * @param priority A syslog priority.<br>
     *                 One of: LOG_EMERG, LOG_ALERT, LOG_CRIT, LOG_ERR,
     *                 LOG_WARNING, LOG_NOTICE, LOG_INFO, and LOG_DEBUG.
     * @param message A null terminated string containing the log message.
     *
     * @see Log::set_callback
     * @see Log
     */
    using log_callback_t = std::function<void (unsigned int priority,
                                               const char* message)>;


    /**
     * The default log callback function used by class Log.
     * This callback calls <code>syslog()</code> to send the log message
     * to the system logger. It does not call <code>openlog()</code>, so
     * if the program using libiomultiplex doesn't call <code>openlog()</code>
     * before using any functionality in libiomultiplex the messages will
     * be logged with default log settings: <code>ident</code> set to
     * <code>NULL</code> and the log facility set to <code>LOG_USER</code>.
     *
     * <b>NOTE:</b> This function is not intended to be used directly,
     * it is set as the default log callback for log messages procuded
     * by the methods in class Log.
     *
     * @param priority A syslog priority.<br>
     *                 One of: LOG_EMERG, LOG_ALERT, LOG_CRIT, LOG_ERR,
     *                 LOG_WARNING, LOG_NOTICE, LOG_INFO, and LOG_DEBUG.
     * @param message A null terminated string containing the log message.
     *
     * @see Log::set_callback
     */
    void default_log_callback (unsigned int priority, const char* message);


    /**
     * This class handles logging in libiomultiplex.
     * Messages are logged with the same priority value used by sylog.
     * The log priority values (in priority order):
     * <dl>
     *     <dt>LOG_EMERG</dt><dd>
     *         system is unusable
     *     </dd>
     *     <dt>LOG_ALERT</dt><dd>
     *         action must be taken immediately
     *     </dd>
     *     <dt>LOG_CRIT</dt><dd>
     *         critical conditions
     *     </dd>
     *     <dt>LOG_ERR</dt><dd>
     *         error conditions
     *     </dd>
     *     <dt>LOG_WARNING</dt><dd>
     *         warning conditions
     *     </dd>
     *     <dt>LOG_NOTICE</dt><dd>
     *         normal, but significant, condition
     *     </dd>
     *     <dt>LOG_INFO</dt><dd>
     *         informational message
     *     </dd>
     *     <dt>LOG_DEBUG</dt><dd>
     *         debug-level message
     *     </dd>
     * </dl>
     *
     * All methods are static and the constructor is
     * deleted to prevent instantiation of this class.
     * @see default_log_callback
     */
    class Log {
    public:
        static constexpr uint default_prio_level = LOG_EMERG;

        Log () = delete; // Prevent instantiation of this class.

        /**
         * Get the current log priority.
         * Messages with lower priority than the current log priority will not be logged.<br/>
         * The log priority values as defined by syslog (in priority order with highest priority first):
         * <dl>
         *     <dt>LOG_EMERG</dt><dd>
         *         system is unusable
         *     </dd>
         *     <dt>LOG_ALERT</dt><dd>
         *         action must be taken immediately
         *     </dd>
         *     <dt>LOG_CRIT</dt><dd>
         *         critical conditions
         *     </dd>
         *     <dt>LOG_ERR</dt><dd>
         *         error conditions
         *     </dd>
         *     <dt>LOG_WARNING</dt><dd>
         *         warning conditions
         *     </dd>
         *     <dt>LOG_NOTICE</dt><dd>
         *         normal, but significant, condition
         *     </dd>
         *     <dt>LOG_INFO</dt><dd>
         *         informational message
         *     </dd>
         *     <dt>LOG_DEBUG</dt><dd>
         *         debug-level message
         *     </dd>
         * </dl>
         *
         * @return The current log priority.
         */
        static unsigned int priority () {
            return prio_level;
        }

        /**
         * Set a new log priority.
         * Messages with lower priority than the current log priority will not be logged.<br/>
         * The log priority values as defined by syslog (in priority order with highest priority first):
         * <dl>
         *     <dt>LOG_EMERG</dt><dd>
         *         system is unusable
         *     </dd>
         *     <dt>LOG_ALERT</dt><dd>
         *         action must be taken immediately
         *     </dd>
         *     <dt>LOG_CRIT</dt><dd>
         *         critical conditions
         *     </dd>
         *     <dt>LOG_ERR</dt><dd>
         *         error conditions
         *     </dd>
         *     <dt>LOG_WARNING</dt><dd>
         *         warning conditions
         *     </dd>
         *     <dt>LOG_NOTICE</dt><dd>
         *         normal, but significant, condition
         *     </dd>
         *     <dt>LOG_INFO</dt><dd>
         *         informational message
         *     </dd>
         *     <dt>LOG_DEBUG</dt><dd>
         *         debug-level message
         *     </dd>
         * </dl>
         *
         * @param priority_threshold The priority threshold for log messages.
         */
        static void priority (unsigned int priority_threshold) {
            prio_level = priority_threshold;
        }

        /**
         * This will log a message with priority LOG_EMERG.
         * @param format A <code>printf</code>-style format string describing
         *               how subsequent arguments are converted
         *               for output.
         */
        static void emerg (const char* format, ...) {
            if (prio_level >= LOG_EMERG) {
                va_list args;
                va_start (args, format);
                log (LOG_EMERG, format, args);
                va_end (args);
            }
        }

        /**
         * This will log a message with priority LOG_ALERT.
         * @param format A <code>printf</code>-style format string describing
         *               how subsequent arguments are converted
         *               for output.
         */
        static void alert (const char* format, ...) {
            if (prio_level >= LOG_ALERT) {
                va_list args;
                va_start (args, format);
                log (LOG_ALERT, format, args);
                va_end (args);
            }
        }

        /**
         * This will log a message with priority LOG_CRIT.
         * @param format A <code>printf</code>-style format string describing
         *               how subsequent arguments are converted
         *               for output.
         */
        static void critical (const char* format, ...) {
            if (prio_level >= LOG_CRIT) {
                va_list args;
                va_start (args, format);
                log (LOG_CRIT, format, args);
                va_end (args);
            }
        }

        /**
         * This will log a message with priority LOG_ERR.
         * @param format A <code>printf</code>-style format string describing
         *               how subsequent arguments are converted
         *               for output.
         */
        static void error (const char* format, ...) {
            if (prio_level >= LOG_ERR) {
                va_list args;
                va_start (args, format);
                log (LOG_ERR, format, args);
                va_end (args);
            }
        }

        /**
         * This will log a message with priority LOG_WARNING.
         * @param format A <code>printf</code>-style format string describing
         *               how subsequent arguments are converted
         *               for output.
         */
        static void warning (const char* format, ...) {
            if (prio_level >= LOG_WARNING) {
                va_list args;
                va_start (args, format);
                log (LOG_WARNING, format, args);
                va_end (args);
            }
        }

        /**
         * This will log a message with priority LOG_NOTICE.
         * @param format A <code>printf</code>-style format string describing
         *               how subsequent arguments are converted
         *               for output.
         */
        static void notice (const char* format, ...) {
            if (prio_level >= LOG_NOTICE) {
                va_list args;
                va_start (args, format);
                log (LOG_NOTICE, format, args);
                va_end (args);
            }
        }

        /**
         * This will log a message with priority LOG_INFO.
         * @param format A <code>printf</code>-style format string describing
         *               how subsequent arguments are converted
         *               for output.
         */
        static void info (const char* format, ...) {
            if (prio_level >= LOG_INFO) {
                va_list args;
                va_start (args, format);
                log (LOG_INFO, format, args);
                va_end (args);
            }
        }

        /**
         * This will log a message with priority LOG_DEBUG.
         * @param format A <code>printf</code>-style format string describing
         *               how subsequent arguments are converted
         *               for output.
         */
        static void debug (const char* format, ...) {
            if (prio_level >= LOG_DEBUG) {
                va_list args;
                va_start (args, format);
                log (LOG_DEBUG, format, args);
                va_end (args);
            }
        }

        /**
         * Set a callback function that handles log messages
         * produced by the methods in this class.
         *
         * <b>NOTE:</b> Do not call this method from inside the
         *              log callback, it will cause a deadlock!
         *
         * @param callback Function that vill be called when
         *                 a message is logged using this class.<br/>
         *                 If set to <code>nullptr</code>, no logging
         *                 will be done.
         *
         * @see default_log_callback
         */
        static void set_callback (log_callback_t callback);


    private:
        static void log (unsigned int priority, const char* format, va_list& args);

        static std::mutex log_mutex;
        static std::atomic_uint prio_level;
        static log_callback_t cb;
    };

}
#endif
