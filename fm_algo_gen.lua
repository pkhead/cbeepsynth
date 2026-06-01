--[[
algorithms:
0.  1 <- (2 3 4)
1.  1 <- (2 3 <- 4)
2.  1 <- 2 <- (3 4)
3.  1 <- (2 3) <- 4
4.  1 <- 2 <- 3 <- 4
5.  1 <- 3  2 <- 4
6.  1  2 <- (3 4)
7.  1  2 <- 3 <- 4
8.  (1 2) <- 3 <- 4
9.  (1 2) <- (3 4)
10. 1  2  3 <- 4
11. (1 2 3) <- 4
12. 1  2  3  4


feedback types:
0.  1 G
1.  2 G
2.  3 G
3.  4 G
4.  1 G  2 G
5.  3 G  4 G
6.  1 G  2 G  3 G
7.  2 G  3 G  4 G
8.  1 G  2 G  3 G  4 G
9.  1 -> 2
10. 1 -> 3
11. 1 -> 4
12. 2 -> 3
13. 2 -> 4
14. 3 -> 4
15. 1 -> 3  2 -> 4
16. 1 -> 4  2 -> 3
17. 1 -> 2 -> 3 -> 4
--]]

-- load BitOp or bit32
local bit
do
    local s
    s, bit = pcall(require, "bit")

    if not s then
        bit = require("bit32")
    end
end

local algorithms = {
    -- 0.
    {
        name = "1 <- (2 3 4)",
        carriers = 1,
        op1 = {2, 3, 4},
    },

    -- 1.
    {
        name = "1 <- (2 3 <- 4)",
        carriers = 1,
        op1 = {2, 3},
        op3 = {4},
    },

    -- 2.
    {
        name = "1 <- 2 <- (3 4)",
        carriers = 1,
        op1 = {2},
        op2 = {3, 4},
    },

    -- 3.
    {
        name = "1 <- (2 3) <- 4",
        carriers = 1,
        op1 = {2, 3},
        op2 = {4},
        op3 = {4},
    },

    -- 4.
    {
        name = "1 <- 2 <- 3 <- 4",
        carriers = 1,
        op1 = {2},
        op2 = {3},
        op3 = {4},
    },

    -- 5.
    {
        name = "1 <- 3  2 <- 4",
        carriers = 2,
        op1 = {3},
        op2 = {4},
    },

    -- 6.
    {
        name = "1  2 <- (3 4)",
        carriers = 2,
        op2 = {3, 4},
    },

    -- 7.
    {
        name = "1  2 <- 3 <- 4",
        carriers = 2,
        op2 = {3},
        op3 = {4},
    },

    -- 8.
    {
        name = "(1 2) <- 3 <- 4",
        carriers = 2,
        op1 = {3},
        op2 = {3},
        op3 = {4},
    },

    -- 9.
    {
        name = "(1 2) <- (3 4)",
        carriers = 2,
        op1 = {3, 4},
        op2 = {3, 4},
    },

    -- 10.
    {
        name = "1  2  3 <- 4",
        carriers = 3,
        op3 = {4},
    },

    -- 11.
    {
        name = "(1 2 3) <- 4",
        carriers = 3,
        op1 = {4},
        op2 = {4},
        op3 = {4},
    },

    -- 12.
    {
        name = "1  2  3  4",
        carriers = 4,
    }
}

local feedback_types = {
    -- 0.
    {
        name = "1 G",
        op1 = 1
    },

    -- 1.
    {
        name = "2 G",
        op2 = 2
    },

    -- 2.
    {
        name = "3 G",
        op3 = 3
    },

    -- 3.
    {
        name = "4 G",
        op4 = 4
    },

    -- 4.
    {
        name = "1 G  2 G",
        op1 = 1,
        op2 = 2
    },

    -- 5.
    {
        name = "3 G  4 G",
        op3 = 3,
        op4 = 4,
    },

    -- 6.
    {
        name = "1 G  2 G  3 G",
        op1 = 1,
        op2 = 2,
        op3 = 3
    },

    -- 7.
    {
        name = "2 G  3 G  4 G",
        op2 = 2,
        op3 = 3,
        op4 = 4,
    },

    -- 8.
    {
        name = "1 G  2 G  3 G  4 G",
        op1 = 1,
        op2 = 2,
        op3 = 3,
        op4 = 4,
    },

    -- 9.
    {
        name = "1 -> 2",
        op2 = 1,
    },

    -- 10
    {
        name = "1 -> 3",
        op3 = 1
    },

    -- 11.
    {
        name = "1 -> 4",
        op4 = 1
    },

    -- 12.
    {
        name = "2 -> 3",
        op3 = 2
    },

    -- 13.
    {
        name = "2 -> 4",
        op4 = 2,
    },

    -- 14
    {
        name = "3 -> 4",
        op4 = 3
    },

    -- 15.
    {
        name = "1 -> 3  2 -> 4",
        op3 = 1,
        op4 = 2
    },

    -- 16.
    {
        name = "1 -> 4  2 -> 3",
        op4 = 1,
        op3 = 2
    },

    -- 17.
    {
        name = "1 -> 2 -> 3 -> 4",
        op4 = 3,
        op3 = 2,
        op2 = 1,
    }
}

local function write_lines_to_file(file_name, lines)
    local f = assert(io.open(file_name, "w"), "could not open file")
    for _, line in ipairs(lines) do
        f:write(line)
        f:write("\n")
    end
    f:close()
end

-- generate header
local algo_signatures = {}
local func_count = 0

do
    local lines = {"#ifndef _fm_algo_h_", "#define _fm_algo_h_", "#include \"fm.h\"", "#include \"../wavetables.h\"", ""}

    for algo_index, _ in ipairs(algorithms) do
        algo_signatures[algo_index] = {}

        for fdbk_index, _ in ipairs(feedback_types) do
            table.insert(algo_signatures[algo_index], ("static double fm_algo%02d%02d(fm_voice_opstate_s *ops, double feedback_amp, const void *userdata)"):format(algo_index-1, fdbk_index-1))
            func_count = func_count + 1
        end
    end

    -- lines[#lines+1] = "typedef double (*fm_algo_f)(fm_voice_s *voice, const float sine_wave[SINE_WAVE_LENGTH+1], const double feedback_amp);"
    lines[#lines+1] = "extern fm_algo_f bbsyn_fm_algorithm_table["..func_count.."];"

    lines[#lines+1] = "#endif"

    write_lines_to_file("synth/src/synth/fm_algo.h", lines)
end

local function generate_sources()
    local lines = {"#include \"fm_algo.h\""}

    table.insert(lines, [[
static inline double fm_calc_op(const float sine_wave[SINE_WAVE_LENGTH+1],
                                const double phase_mix) {
    const int phase_int = (int) phase_mix;
    const int index = phase_int & (SINE_WAVE_LENGTH - 1);
    const double sample = sine_wave[index];
    return sample + (sine_wave[index+1] - sample) * (phase_mix - phase_int);
}
    ]])

    for algo_index, algo_data in ipairs(algorithms) do    
        for fdb_index, fdb_data in ipairs(feedback_types) do
            lines[#lines+1] = algo_signatures[algo_index][fdb_index] .. " {"
            for op=4, 1, -1 do
                local mod_gen_list = {}
                for _, mod_op in ipairs(algo_data["op" .. op] or {}) do
                    mod_gen_list[#mod_gen_list+1] = " + "
                    mod_gen_list[#mod_gen_list+1] = "op"..(mod_op-1).."_scaled"
                end
                local mod_gen = table.concat(mod_gen_list)

                local fdb_str
                if fdb_data["op"..op] then
                    fdb_str = " + feedback_amp * ops["..(fdb_data["op"..op]-1).."].output"
                else
                    fdb_str = ""
                end

                lines[#lines+1] = "    double op"..(op-1).."_scaled = ops["..(op-1).."].expression * (ops[" .. (op-1) .. "].output = fm_calc_op("
                lines[#lines+1] = "        userdata, ops["..(op-1).."].phase" .. mod_gen .. fdb_str
                lines[#lines+1] = "    ));"
            end

            local carriers = {}
            for i=1, algo_data.carriers do
                table.insert(carriers, "op"..(i-1).."_scaled")
            end

            lines[#lines+1] = "    return " .. table.concat(carriers, " + ") .. ";"
            lines[#lines+1] = "}"
        end
    end

    lines[#lines+1] = "fm_algo_f bbsyn_fm_algorithm_table["..(func_count).."] = {"

    for algo_index, algo_data in ipairs(algorithms) do
        for fdb_index, fdb_data in ipairs(feedback_types) do
            lines[#lines+1] = ("    fm_algo%02d%02d,"):format(algo_index - 1, fdb_index - 1)
        end
    end

    lines[#lines+1] = "};"
    
    write_lines_to_file("synth/src/synth/fm_algo.c", lines)
end

local function generate_ampmod_switch(algos, out)
    for algo_index, algo_data in ipairs(algos) do
        -- case {case_number}: // {case_name}
        out:write("case ")
        out:write(algo_index - 1)
        out:write(": // ")
        out:write(algo_data.name)
        out:write("\n")

        -- carriers = {carriers};
        out:write("    carriers = ")
        out:write(algo_data.carriers)
        out:write(";\n")

        for i=1, 4 do
            -- desc.mod[{i-1}] = {mod bitfield};
            local opdat = algo_data["op"..i]

            if opdat then
                local modf = 0
                for _, inp in ipairs(opdat) do
                    modf = bit.bor(modf, bit.lshift(1, inp - 1))
                end

                if modf ~= 0 then
                    out:write("    desc.mod[")
                    out:write(i - 1)
                    out:write("] = ")
                    out:write(string.format("0x%x", modf))
                    out:write(";\n")
                end
            end
        end

        out:write("    break;\n\n")
    end
end

local function generate_fdb_switch(fdb, out)
    for fdb_idx, fdb_data in ipairs(fdb) do
        -- case {case_number}: // {case_name}
        out:write("case ")
        out:write(fdb_idx - 1)
        out:write(": // ")
        out:write(fdb_data.name)
        out:write("\n")

        for i=1, 4 do
            -- desc.fdb[{i-1}] = {fdb bitfield};
            local opdat = fdb_data["op"..i]

            if opdat then
                local modf = bit.lshift(1, opdat - 1)
                if modf ~= 0 then
                    out:write("    desc.fdb[")
                    out:write(i - 1)
                    out:write("] = ")
                    out:write(string.format("0x%x", modf))
                    out:write(";\n")
                end
            end
        end

        out:write("    break;\n\n")
    end
end

---@return file*?, boolean?
local function parse_out_file(path)
    if not arg[2] then
        io.stderr:write("error: output file not given\n")
        return
    end

    if arg[2] == "-" then
        return io.stdout, false
    else
        local f = io.open(arg[2], "w")
        if not f then
            io.stderr:write("error: could not open " .. arg[2])
            return
        end

        return f, true
    end
end

if arg[1] == "--amp-switch" then
    local f, close = parse_out_file(arg[2])
    if not f then
        os.exit(1)
    end

    generate_ampmod_switch(algorithms, f)

    if close then f:close() end
elseif arg[1] == "--fdb-switch" then
    local f, close = parse_out_file(arg[2])
    if not f then
        os.exit(1)
    end

    generate_fdb_switch(feedback_types, f)

    if close then f:close() end
else
    generate_sources()
end