library IEEE;
USE ieee.std_logic_1164.all;
USE ieee.std_logic_arith.all;
USE ieee.std_logic_unsigned.all;
use std.textio.all;
use IEEE.std_logic_textio.all;

--Library lattice;
--use lattice.components.all;

library machxo2;
use machxo2.components.all;

--library work;
--use work.top_pkg.all;


--emulates NJU6575A buffer functionality and interfaces with rp2040
entity top_sim is

end top_sim;

architecture sim of top_sim is
type t_bus is record
  dt        : std_logic_vector(7 downto 0);
  wrn       : std_logic;
  rdn       : std_logic;
  a0        : std_logic;
end record;
component vd5000disp is
  port (
          dt          : inout std_logic_vector(7 downto 0);
          a0          : in    std_logic;
          wrn         : in    std_logic;
          rdn         : in    std_logic;
          sp_clk      : out   std_logic;
          sp_dt       : out   std_logic_vector(3 downto 0);
          sp_csn      : out   std_logic
  );
end component;
constant clk_period                 : time := 400 ns;
constant  cNOP :std_logic_vector(2 downto 0) :="111";
constant  cRDN :std_logic_vector(2 downto 0) :="110";
constant  cWRN :std_logic_vector(2 downto 0) :="101";
constant  cCMD :std_logic_vector(2 downto 0) :="001";
signal intnl_bus                    : t_bus;
signal clk_tst                      : std_logic;
signal sp_clk, sp_csn               : std_logic;
signal a0                           : std_logic:='0';
signal sp_dt                        : std_logic_vector(3 downto 0);
signal dt_int                       : std_logic_vector(7 downto 0):=(others => '0');
signal clk_cnt                      : std_logic_vector(31 downto 0):=(others=>'0');
signal tst_cnt                      : std_logic_vector(2 downto 0):=(others=>'0');
signal cell_cnt                     : std_logic_vector(10 downto 0):=(others => '0');
signal cmd_string_s                 : string(1 to 4);
function set_bus(bus_crtl:std_logic_vector(2 downto 0); bus_dt:std_logic_vector(7 downto 0)) return t_bus is
variable result:t_bus;
begin
  result.a0  := bus_crtl(2);
  result.wrn := bus_crtl(1);
  result.rdn := bus_crtl(0);
  result.dt  := bus_dt;
  return result;
end function;

function parse_line(cmd:string;bus_dt:std_logic_vector(7 downto 0)) return t_bus is
variable result:t_bus;
variable cmd_string : string(1 to 4);
variable tmp_line: line;
variable bus_crtl: std_logic_vector(2 downto 0);
begin
  cmd_string := cmd(1 to 4);
  if cmd_string = "cWRN" then
    bus_crtl := cWRN;
  elsif cmd_string = "cRDN" then
    bus_crtl := cRDN;
  elsif cmd_string = "cCMD" then
    bus_crtl := cCMD;
  else
    bus_crtl := cNOP;
  end if;

  result.a0  := bus_crtl(2);
  result.wrn := bus_crtl(1);
  result.rdn := bus_crtl(0);
  result.dt  := bus_dt;
  return result;
end function;

File data_File: text open read_mode is "sim_input.txt";
begin
   -- data clock
   clk_process_2 :process
   begin
        clk_tst <= '0';
        wait for clk_period/2;
        clk_tst <= '1';
        wait for clk_period/2;
   end process;
   
   process(clk_tst)
   variable L_In: Line;
   variable cmd_string : string(1 to 4);
   variable ch_space:character;
   variable bus_dt:std_logic_vector(7 downto 0);
   variable tmp: STD_LOGIC_VECTOR(7 downto 0);
   begin
     if rising_edge(clk_tst) then
        tst_cnt <= tst_cnt + 1;
        if conv_integer(tst_cnt) = 7 then
          if(not endfile(data_File)) then
            readline(data_File,L_In); 
            read(L_In, cmd_string);
            cmd_string_s<= cmd_string;
            read(L_In, ch_space);
            hread(L_In,bus_dt);
            intnl_bus <= parse_line(cmd_string,bus_dt);
          end if;
        else
          bus_dt:=x"00";
          intnl_bus <= parse_line("cNOP",bus_dt);
        end if;
     end if;
   end process;
   
   u_ld5000_disp: component vd5000disp
   port map(
   dt       => intnl_bus.dt,
   a0       => intnl_bus.a0,
   wrn      => intnl_bus.wrn,
   rdn      => intnl_bus.rdn,
   sp_clk   => sp_clk,
   sp_dt    => sp_dt,
   sp_csn   => sp_csn
   );
  
end architecture;