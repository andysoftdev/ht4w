/** -*- js -*-
 * Copyright (C) 2011 Andy Thalmann
 *
 * This file is part of ht4w.
 *
 * ht4w is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
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

function echo(msg) {
    WScript.Echo(msg);
}

function write(msg) {
    WScript.Stdout.Write(msg);
}

function writeln(msg) {
    WScript.Stdout.WriteLine(msg);
}

function sleep(msec) {
    WScript.Sleep(msec);
}

function throw_if_null(arg) {
    if (arg == null) {
        throw "Invalid argument [null]";
    }
}

function format(number) {
    return (number < 10 ? ' ' : '') + number.toString();
}

function trim(s) {
    return s.replace(/^\s+|\s+$/g, "");
}

function is_null_or_empty(s) {
    return s == null || s == "";
}
function folder_exists(path) {
    throw_if_null(path);
    throw_if_null(fso);
    return fso.FolderExists(path);
}

function file_exists(file) {
    throw_if_null(file);
    throw_if_null(fso);
    return fso.FileExists(file);
 }

 function file_copy(src, dst) {
    throw_if_null(src);
    throw_if_null(dst);
    throw_if_null(fso);    
    try {
        fso.CopyFile(src, dst, true); // Overwrite existing
    }
    catch (e) {
        throw "Copy file " + src + " to " + dst + " failed";
    }
}

function files_copy(src, files, dst) {
    throw_if_null(src);
    throw_if_null(files);
    throw_if_null(dst);
    throw_if_null(fso);
    for (var n in files) {
        file_copy(src + "\\" + files[n], dst);
    }
}

function file_delete(file) {
    throw_if_null(file);
    throw_if_null(fso);
    try {
        fso.DeleteFile(file);
    }
    catch (e) {
    }
}

function files_delete(files) {
    throw_if_null(files);
    throw_if_null(fso);
    for (var n in files) {
        file_delete(files[n]);
    }
}

function folder_find(root, file) {
    throw_if_null(root);
    throw_if_null(file);
    throw_if_null(fso);
    if (file_exists(root + "\\" + file)) {
        return root;
    }
    var folder = fso.GetFolder(root);
    var subfolders = new Enumerator(folder.SubFolders);
    for (; !subfolders.atEnd(); subfolders.moveNext()) {
        var result = folder_find(subfolders.item(), file);
        if (result != null) {
            return result;
        }
    }
    return null;
}

function system(cmd, out, err) {
    throw_if_null(cmd);
    throw_if_null(wshshell);
    var cmdline = cmd;
    if (out != null) {
        cmdline = cmdline + " > " + out;
        if (err != null) {
            cmdline = cmdline + " 2> " + err;
        }
    }
    var count = 0;
    var exec = wshshell.Exec("cmd /C " + cmdline);
    while (exec.Status == 0 && count < timeout/200) {
        WScript.Sleep(200);
        count++;
    }
    if (count >= timeout / 200) {
        wshshell.Run("taskkill /F /T /PID:" + exec.ProcessID, 0, true);
        write(" timed out");
        return -1;
    }
    return exec.ExitCode;
}

function system_nowait(cmd, out, err) {
    throw_if_null(cmd);
    throw_if_null(wshshell);
    var cmdline = cmd;
    if (out != null) {
        cmdline = cmdline + " > " + out;
        if (err != null) {
            cmdline = cmdline + " 2> " + err;
        }
    }
    wshshell.Exec("cmd /C " + cmdline);
}

function system_log(cmd, logfile, testName) {
    throw_if_null(wshshell);
    if (!is_null_or_empty(testName)) {
        system_log("echo +" + testName, logfile);
    }
    var count = 0;
    var exec = wshshell.Exec("cmd /C " + cmd + " >> " + logfile);
    while (exec.Status == 0 && count < timeout / 200) {
        WScript.Sleep(200);
        count++;
    }
    if (count >= timeout / 200) {
        wshshell.Run("taskkill /F /T /PID:" + exec.ProcessID, 0, true);
        write(" timed out");
        return -1;
    }
    return exec.ExitCode;
}

function prepare_target(testName, files) {
    var folder = folder_find(solutionDir + "\\src\\cc", testName + ".vcxproj");
    if (folder == null) {
        throw "Test project " + testName + ".vcxproj does not exist";
    }
    files_copy(folder, files, targetDir);
}

function clean_target() {
    system("..\\hypertable.service.exe --kill-servers");
    files_delete(["*.txt", "*.xml", "*.out", "*.output.*", "*.stdout", "*.stderr", "*.golden", "*.cfg", "*.spec", "*.gz", "*.tsv", "*.dat"]);
    system("rd /S /Q test");
    system("rd /S /Q hypertable");
    system("rd /S /Q hyperspace");
    system("rd /S /Q fs");
    system("rd /S /Q run");
    system("rd /S /Q log");
}

function run_target(logfile, testName, args, noclean) {
    var exe = targetDir + "\\" + testName + ".exe";
    if (!file_exists(exe)) {
        throw "File does not exist [" + exe + "]";
    }
    if (args != null) {
        exe = exe + " " + args;
    }
    try {
        var status = system_log(exe, logfile, testName);
        if (noclean != null) {
            if (noclean) {
                return status;
            }
        }
        clean_target();
        return status;
    }
    catch (e) {
        clean_target();
        throw e;
    }
}

function run_servers(args) {
    system("..\\hypertable.service --kill-servers");
    system("rd /S /Q hyperspace");
    system("rd /S /Q hsroot");
    system("rd /S /Q fs");
    system("rd /S /Q run");
    system("rd /S /Q log");
    var status = system("..\\hypertable.service --start-servers " + args);
    if (status != 0) {
        throw "Unable to start servers [" + args +"]";
    }
    file_copy(targetDir + "\\..\\conf\\hypertable.cfg", targetDir);
}


// getters
function get_solution_path(args) {
    throw_if_null(args);
    if (args.Length <= 0) {
        throw "Missing argument, solution path not specified";
    }
    var solutionDir = args(0);
    if (!folder_exists(solutionDir)) {
        throw "Solution path does not exist [" + solutionDir + "]";
    }
    return solutionDir;
}

function get_target_path(args) {
    throw_if_null(args);
    if (args.Length <= 1) {
        throw "Missing argument, target path not specified";
    }
    var targetDir = args(1);
    if (!folder_exists(targetDir)) {
        throw "Target path does not exist [" + targetDir + "]";
    }
    return targetDir;
}

function get_test_filter(args) {
    throw_if_null(args);
    var re = null;
    if (args.Length > 2) {
        re = args(2);
    }
    return re;
}


// tests
function async_api_test(logfile, testName) {
    prepare_target(testName, ["asyncApiTest.golden"]);
    run_servers("--no-thriftbroker --Hypertable.DataDirectory=" + targetDir);
    return run_target(logfile, testName);
}

function bdb_fs_test(logfile, testName) {
    prepare_target(testName, ["bdb_fs_test.golden"]);
    return run_target(logfile, testName);
}

function bmz_test(logfile, testName) {
    var status = run_target(logfile, testName, "--file ..\\Hypertable.exe");
    if (status != 0) {
        return status;
    }
    return run_target(logfile, testName, "--file ..\\Hypertable.pdb");
}

function cellstore_scanner_delete_test(logfile, testName) {
    prepare_target(testName, ["cellstorescanner_delete_test.golden"]);
    system_nowait("..\\Hypertable.LocalBroker.exe --debug --DfsBroker.Local.Root=" + targetDir, "localbroker.stdout", "localbroker.stderr");
    sleep(500);
    return run_target(logfile, testName);
}

function cellstore_scanner_test(logfile, testName) {
    prepare_target(testName, ["cellstorescanner_test.golden"]);
    system_nowait("..\\Hypertable.LocalBroker.exe --debug --DfsBroker.Local.Root=" + targetDir, "localbroker.stdout", "localbroker.stderr");
    sleep(500);
    return run_target(logfile, testName);
}

function cellstore64_test(logfile, testName) {
    prepare_target(testName, ["cellstore64_test.golden"]);
    system_nowait("..\\Hypertable.LocalBroker.exe --debug --DfsBroker.Local.Root=" + targetDir, "localbroker.stdout", "localbroker.stderr");
    sleep(500);
    return run_target(logfile, testName);
}

function comm_datagram_test(logfile, testName) {
    system(solutionDir + "\\gzip -d -c " + solutionDir + "\\tests\\data\\words.gz", "words");
    system(solutionDir + "\\head -50000 words", "commTestDatagram.golden");
    var status = run_target(logfile, testName);
    file_delete("words");
    return status;
}

function comm_reverse_request_test(logfile, testName) {
    prepare_target(testName, ["commTestReverseRequest.golden", "datafile.txt"]);
    return run_target(logfile, testName);
}

function comm_test(logfile, testName) {
    system(solutionDir + "\\gzip -d -c " + solutionDir + "\\tests\\data\\words.gz", "words");
    system(solutionDir + "\\head -50000 words", "commTest.golden");
    var status = run_target(logfile, testName);
    file_delete("words");
    return status;
}

function comm_timeout_test(logfile, testName) {
    prepare_target(testName, ["commTestTimeout.golden"]);
    return run_target(logfile, testName);
}

function comm_timer_test(logfile, testName) {
    prepare_target(testName, ["commTestTimer.golden"]);
    var status = run_target(logfile, testName);
    files_delete(["commTestTimer*"]);
    return status;
}

function commit_log_test(logfile, testName) {
    system_nowait("..\\Hypertable.LocalBroker.exe --debug --DfsBroker.Local.Root=" + targetDir, "localbroker.stdout", "localbroker.stderr");
    sleep(500);
    return run_target(logfile, testName);
}

function compressor_test(logfile, testName) {
    var status = 1;
    prepare_target(testName, ["good-schema-1.xml"]);
    var args = ["bmz", "lzo", "zlib", "quicklz", "none"];
    for (var n in args) {
        status = run_target(logfile, testName, args[n], true);
        if (status != 0) {
            clean_target();
            return status;
        }
    }
    clean_target();
    return status;
}

function container_test(logfile, testName) {
    return run_target(logfile, testName, "--components smalldeque smallvector bigdeque bigvector");
}

function fileblock_cache_test(logfile, testName) {
    return run_target(logfile, testName, "--total-memory=10000000");
}

function future_abrupt_end_test(logfile, testName) {
    file_copy(solutionDir + "\\tests\\integration\\future-abrupt-end\\data.spec", targetDir);
    run_servers("--no-thriftbroker --Hypertable.RangeServer.Scanner.BufferSize=128K --Hypertable.DataDirectory=" + targetDir);
    var status = run_target(logfile, testName, "10 100 1 0", true);
    if (status != 0) {
        clean_target();
        return status;
    }
    status = run_target(logfile, testName, "10 100 2 0", true);
    if (status != 0) {
        clean_target();
        return status;
    }
    status = run_target(logfile, testName, "10 100 4 0", true);
    if (status != 0) {
        clean_target();
        return status;
    }
    status = run_target(logfile, testName, "10 100 1 6", true);
    if (status != 0) {
        clean_target();
        return status;
    }
    status = run_target(logfile, testName, "10 100 2 6", true);
    if (status != 0) {
        clean_target();
        return status;
    }
    return run_target(logfile, testName, "10 100 4 6");
}

function future_mutator_cancel_test(logfile, testName) {
    prepare_target(testName, ["future_test.cfg"]);
    run_servers("--no-thriftbroker --Hypertable.DataDirectory=" + targetDir);
    var status = run_target(logfile, testName, "1 100 10000", true);
    if (status != 0) {
        clean_target();
        return status;
    }
    status = run_target(logfile, testName, "2 100 10000", true);
    if (status != 0) {
        clean_target();
        return status;
    }
    status = run_target(logfile, testName, "4 10 10000", true);
    if (status != 0) {
        clean_target();
        return status;
    }
    status = run_target(logfile, testName, "8 1000 100000", true);
    if (status != 0) {
        clean_target();
        return status;
    }
    status = run_target(logfile, testName, "8 100 100000", true);
    if (status != 0) {
        clean_target();
        return status;
    }
    return run_target(logfile, testName, "8 10000 200000");
}

function future_test(logfile, testName) {
    prepare_target(testName, ["future_test.cfg"]);
    file_copy(solutionDir + "\\tests\\integration\\future-abrupt-end\\data.spec", targetDir);
    run_servers("--no-thriftbroker --config=./future_test.cfg --Hypertable.DataDirectory=" + targetDir);
    return run_target(logfile, testName);
}

function hyperspace_test(logfile, testName) {
    prepare_target(testName, ["hyperspaceTest.cfg", "client1.golden", "client2.golden", "client3.golden"]);
    return run_target(logfile, testName);
}

function hypertable_ldi_select_test(logfile, testName) {
    file_copy(solutionDir + "\\gzip.exe", targetDir);
    prepare_target(testName, ["hypertable_ldi_*.hql", "hypertable_*.golden", "hypertable_test.tsv.gz", "hypertable_escape_test.tsv"]);
    run_servers("--no-thriftbroker --Hypertable.DataDirectory=" + targetDir);
    system("..\\hypertable.exe  -e \"create namespace '/test';quit;\"");
    var status = run_target(logfile, testName, targetDir);
    files_delete(["gzip.exe", "dfs_select"]);
    return status;
}

function hypertable_test(logfile, testName) {
    file_copy(solutionDir + "\\gzip.exe", targetDir);
    file_copy(solutionDir + "\\sed.exe", targetDir);
    prepare_target(testName, ["hypertable_test.hql", "hypertable_test.golden", "hypertable_select_gz_test.golden", "*.tsv"]);
    run_servers("--no-thriftbroker --Hypertable.DataDirectory=" + targetDir);
    var status = run_target(logfile, testName);
    files_delete(["gzip.exe", "sed.exe"]);
    return status;
}

function init_test(logfile, testName) {
    prepare_target(testName, ["init_test.golden"]);
    var status = system(testName + ".exe  --i16 1k --i32 64K --i64 1G --boo", "init_test.out");
    if (status == 0) {
        status = system("fc init_test.out init_test.golden");
    }
    clean_target();
    return status;
}

function large_insert_test(logfile, testName) {
    run_servers("--no-thriftbroker --Hypertable.DataDirectory=" + targetDir);
    system("..\\hypertable.exe  -e \"create namespace '/test';quit;\"");
    return run_target(logfile, testName, "34587");
}

function load_datasource_test(logfile, testName) {
    prepare_target(testName, ["loadDataSourceTest*.golden", "loadDataSourceTest*.dat"]);
    return run_target(logfile, testName);
}

function location_cache_test(logfile, testName) {
    file_copy(solutionDir + "\\tests\\data\\random.dat", targetDir);
    prepare_target(testName, ["locationCacheTest.golden"]);
    return run_target(logfile, testName);
}

function metalog_test(logfile, testName) {
    prepare_target(testName, ["metalog_test*.golden"]);
    system_nowait("..\\Hypertable.LocalBroker.exe --debug --DfsBroker.Local.Root=" + targetDir, "localbroker.stdout", "localbroker.stderr");
    sleep(500);
    return run_target(logfile, testName, "--DfsBroker.Host=localhost --Hypertable.DataDirectory=" + targetDir);
}

function mutator_nolog_sync_test(logfile, testName) {
    prepare_target(testName, ["MutatorNoLogSyncTest.cfg"]);
    run_servers("--no-thriftbroker --no-rangeserver --config=./MutatorNoLogSyncTest.cfg --Hypertable.DataDirectory=" + targetDir);
    return run_target(logfile, testName, targetDir + "\\..\\");
}

function name_id_mapper_test(logfile, testName) {
   prepare_target(testName, ["name_id_mapper_test.cfg"]);
   return run_target(logfile, testName);

}

function op_dependency_test(logfile, testName) {
    run_servers("--no-thriftbroker --Hypertable.DataDirectory=" + targetDir);
    return run_target(logfile, testName, "--config=./hypertable.cfg --Hypertable.DataDirectory=" + targetDir);
}

function periodic_flush_test(logfile, testName) {
    run_servers("--no-thriftbroker --Hypertable.DataDirectory=" + targetDir);
    return run_target(logfile, testName, "--config=./hypertable.cfg --Hypertable.DataDirectory=" + targetDir);
}

function properties_test(logfile, testName) {
    prepare_target(testName, ["properties_test.golden"]);
    var status = system(testName + ".exe", "properties_test.out");
    if (status == 0) {
        status = system("fc properties_test.out properties_test.golden");
    }
    clean_target();
    return status;
}

function random_read_test(logfile, testName) {
    run_servers("--no-thriftbroker --Hypertable.DataDirectory=" + targetDir);
    var status = system("random_write_test.exe --total-bytes=250000 --config=./hypertable.cfg --Hypertable.DataDirectory=" + targetDir);
    if (status == 0) {
        status = run_target(logfile, testName, "--total-bytes=250000 --config=./hypertable.cfg --Hypertable.DataDirectory=" + targetDir);
    }
    else {
        clean_target();
    }
    return status;
}

function row_delete_test(logfile, testName) {
    run_servers("--no-thriftbroker --Hypertable.DataDirectory=" + targetDir);
    return run_target(logfile, testName, "34587");
}

function scanner_abrupt_end_test(logfile, testName) {
    run_servers("--no-thriftbroker --Hypertable.DataDirectory=" + targetDir);
    var status = run_target(logfile, testName, "10", true);
    if (status != 0) {
        clean_target();
        return status;
    }
    status = run_target(logfile, testName, "100", true);
    if (status != 0) {
        clean_target();
        return status;
    }
    status = run_target(logfile, testName, "1000", true);
    if (status != 0) {
        clean_target();
        return status;
    }
    status = run_target(logfile, testName, "10000", true);
    if (status != 0) {
        clean_target();
        return status;
    }
    return run_target(logfile, testName, "100000");
}

function schema_test(logfile, testName) {
    prepare_target(testName, ["*-schema-*.xml", "schemaTest.golden"]);
    return run_target(logfile, testName);
}


// all tests
var all_tests = new ActiveXObject("Scripting.Dictionary");
all_tests.add("accessgroup_garbage_tracker_test", run_target);
all_tests.add("async_api_test", async_api_test);
all_tests.add("bdb_fs_test", bdb_fs_test);
all_tests.add("bloom_filter_test", run_target);
all_tests.add("bmz_test", bmz_test);
all_tests.add("cellstore_scanner_delete_test", cellstore_scanner_delete_test);
all_tests.add("cellstore_scanner_test", cellstore_scanner_test);
//FIXME all_tests.add("cellstore64_test", cellstore64_test);
all_tests.add("comm_datagram_test", comm_datagram_test);
all_tests.add("comm_reverse_request_test", comm_reverse_request_test);
all_tests.add("comm_test", comm_test);
all_tests.add("comm_timeout_test", comm_timeout_test);
all_tests.add("comm_timer_test", comm_timer_test);
all_tests.add("commit_log_test", commit_log_test);
all_tests.add("compressor_test", compressor_test);
all_tests.add("container_test", container_test);
all_tests.add("escape_test", run_target);
all_tests.add("escaper_test", run_target);
all_tests.add("exception_test", run_target);
all_tests.add("fileblock_cache_test", fileblock_cache_test);
all_tests.add("future_abrupt_end_test", future_abrupt_end_test);
all_tests.add("future_mutator_cancel_test", future_mutator_cancel_test);
all_tests.add("future_test", future_test);
all_tests.add("hash_test", run_target);
all_tests.add("hyperspace_test", hyperspace_test);
all_tests.add("hypertable_ldi_select_test", hypertable_ldi_select_test);
all_tests.add("hypertable_test", hypertable_test);
all_tests.add("inetaddr_test", run_target);
all_tests.add("init_test", init_test);
all_tests.add("large_insert_test", large_insert_test);
all_tests.add("load_datasource_test", load_datasource_test);
all_tests.add("location_cache_test", location_cache_test);
all_tests.add("logging_test", run_target);
all_tests.add("md5_base64_test", run_target);
all_tests.add("metalog_test", metalog_test);
all_tests.add("mutator_nolog_sync_test", mutator_nolog_sync_test);
all_tests.add("mutex_test", run_target);
all_tests.add("name_id_mapper_test", name_id_mapper_test);
all_tests.add("op_dependency_test", op_dependency_test);
all_tests.add("pagearena_test", run_target);
all_tests.add("periodic_flush_test", periodic_flush_test);
all_tests.add("properties_test", properties_test);
all_tests.add("query_cache_test", run_target);
all_tests.add("random_read_test", random_read_test);
all_tests.add("rangeserver_serialize_test", run_target);
all_tests.add("row_delete_test", row_delete_test);
all_tests.add("scanner_abrupt_end_test", scanner_abrupt_end_test);
all_tests.add("schema_test", schema_test);
all_tests.add("scope_guard_test", run_target);
all_tests.add("serialization_test", run_target);
all_tests.add("stats_serialize_test", run_target);
all_tests.add("string_compressor_test", run_target);
all_tests.add("tableid_cache_test", run_target);
all_tests.add("timeinline_test", run_target);

// globals
var wshshell = new ActiveXObject("WScript.Shell");
var fso = new ActiveXObject("Scripting.FileSystemObject")
var timeout = 240000; // [ms]

var solutionDir = null;
var targetDir = null;
var testFilter = null;

// main
try {
    var tests_completed = 0;
    var tests_failed = 0;

    solutionDir = get_solution_path(WScript.Arguments);
    targetDir = get_target_path(WScript.Arguments);
    testFilter = get_test_filter(WScript.Arguments);
    echo("ht4w test runner @" + targetDir);

    // logfile
    var logfile = WScript.ScriptFullName + ".log";
    if (file_exists(logfile)) {
        file_delete(logfile);
    }

    // clean target
    clean_target();

    // invoke all tests
    var all_test_keys = (new VBArray(all_tests.Keys())).toArray();
    var all_test_count = all_test_keys.length;
    for (var n in all_test_keys) {
        if (testFilter != null && !all_test_keys[n].match(testFilter)) {
            continue;
        }
        var preffix = format(Number(n) + 1) + "/" + format(all_test_count) + " " + all_test_keys[n];
        write(preffix);

        // cwd
        wshshell.CurrentDirectory = targetDir;

        // run test
        var result = all_tests.item(all_test_keys[n])(logfile, all_test_keys[n]);
        if (result == 0) {
            writeln(" - passed");
            ++tests_completed;
        }
        else {
            writeln(" - failed");
            ++tests_failed;
        }
    }

    echo("");
    echo("tests complete: " + format(tests_completed));
    echo("tests failed:   " + format(tests_failed));
    echo("");
    WScript.Quit(tests_failed);
}
catch (e) {
    echo("")
    echo(e.toString());
    WScript.Quit(-1);
}