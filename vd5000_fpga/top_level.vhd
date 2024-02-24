library IEEE;
USE ieee.std_logic_1164.all;
USE ieee.std_logic_arith.all;
USE ieee.std_logic_unsigned.all;
library machxo2;
use machxo2.components.all;

--emulates NJU6575A buffer functionality and interfaces with rp2040
entity vd5000disp is
  port (
          dt          : inout std_logic_vector(7 downto 0);
          a0          : in    std_logic;
          wrn         : in    std_logic;
          rdn         : in    std_logic;
          sp_clk      : out   std_logic;
          sp_dt       : out   std_logic_vector(3 downto 0);
          sp_csn      : out   std_logic;
          dbg         : out   std_logic
  );
end vd5000disp;

architecture rtl of vd5000disp is
  type t_mem is array (2047 downto 0) of std_logic_vector(7 downto 0);
  type t_id_line is array (3 downto 0) of std_logic_vector(7 downto 0);
  signal buf          : t_mem;
  constant cGATE_VAL  :integer:=1; --must be even


  
  --built in oscilator component
  component osch
  -- synthesis translate_off
  generic (NOM_FREQ: string := "2.08");
  -- synthesis translate_on
  port (
        stdby        : in  std_logic;
        osc          : out std_logic;
        sedstdby     : out std_logic);
  end component;
  --133.00 88.67 66.50 53.20 44.33 38.00 33.25 29.56 19.00 14.00
  attribute NOM_FREQ : string;
  attribute NOM_FREQ of OSCinst0 : label is "38.00";
  
  signal clk_int                    : std_logic;
  signal wr_line, rd_line           : std_logic_vector(2 downto 0);
  signal a0_line, trn_cnt           : std_logic_vector(2 downto 0);
  signal cpu_wr, cpu_rd, cpu_a0     : std_logic;
  signal dti_int, dto_int           : std_logic_vector(7 downto 0);
  signal clk_gate_cnt               : std_logic_vector(7 downto 0):=(others=>'0');
  signal flags                      : std_logic_vector(7 downto 0):=(others=>'0'); --mode flags
  signal pg_addr                    : std_logic_vector(2 downto 0);
  signal col_addr, col_addr_rmw     : std_logic_vector(7 downto 0);
  signal dto_mem                    : std_logic_vector(7 downto 0);
  signal addr_lock                  : std_logic:='0';
  signal buf_wr, buf_rd             : std_logic;
  signal tst_cnt                    : std_logic_vector(7 downto 0);
  signal do_spi                     : std_logic:='0';
  signal do_spi_1, spi_done         : std_logic;
  signal clk_en, sp_clk_int,clk_en_1: std_logic;
  signal sp_cs_int                  : std_logic;
  signal dt2spi                     : std_logic_vector(15 downto 0);
  signal dt2spi_i                   : std_logic_vector(15 downto 0);
  signal id_line                    : t_id_line;
  signal id_page, id_colh, id_coll  : std_logic;
  signal id_page_0, id_colh_0, id_coll_0  : std_logic;
  signal id_page_1, id_colh_1, id_coll_1  : std_logic;
  signal id_fin,id_fin_1, id_fin_0  : std_logic;
  signal id_tst                     : std_logic;
  signal cmd_issued                 : std_logic;
begin
  sp_csn <= sp_cs_int;
  sp_clk <= sp_clk_int;
  
  
  cpu_wr   <= '1' when conv_integer(wr_line(2 downto 1)) = 0 else '0';
  cpu_rd   <= '1' when conv_integer(rd_line(2 downto 1)) = 0 else '0';
  cpu_a0   <= '1' when conv_integer(a0_line(2 downto 1)) = 3 else '0';
 
  
  dto_int <= "0000000"&dto_mem(0) when pg_addr = "100" else dto_mem;
  dt <= dto_int when cpu_rd = '1' else (others => 'Z');
  cmd_issued <= '1' when cpu_a0 = '0' and (wr_line(1)='0' and wr_line(2)='1') else '0';
  --below signals are mainly for debug with Lattice reveal
  dbg <= id_fin;
  id_fin   <= '1' when id_fin_0 = '1' and id_fin_1 = '0' else '0';
  id_fin_0 <= id_page and id_colh and id_coll; 
  id_page  <= '1' when id_page_0 = '1' and id_page_1 = '0' else '0';
  id_colh  <= '1' when id_colh_0 = '1' and id_colh_1 = '0' else '0';
  id_coll  <= '1' when id_coll_0 = '1' and id_coll_1 = '0' else '0';
  
  
  process(clk_int)
  begin
    if rising_edge(clk_int) then
      id_fin_1  <= id_fin_0;
      id_page_1 <= id_page_0;
      id_colh_1 <= id_colh_0; 
      id_coll_1 <= id_coll_0; 
      if cmd_issued='1' then
        id_line(0) <= dti_int;
        for i in id_line'left downto 1 loop
          id_line(i) <= id_line(i-1);
        end loop;
      end if;
      -- looking for pg, colh, coll sequence
      if (id_line(2)(7 downto 4) = "1011") then id_page_0 <='1'; else id_page_0 <='0'; end if;
      if (id_line(1)(7 downto 4) = "0001") then id_colh_0 <='1'; else id_colh_0 <='0'; end if;
      if (id_line(0)(7 downto 4) = "0000") then id_coll_0 <='1'; else id_coll_0 <='0'; end if;
    end if;
  end process;
    
  --command processing
  --flags
  -- bit 0 - display on/off
  -- bit 1 - adc select
  -- bit 2 - 0-normal, 1-inverse
  -- bit 3 - whole display on/off
  -- bit 4 - icon display, 0- no icon, 1 - icon
  -- bit 5 - read modify write, if 1 read does not autoincrement col addr.
  process(clk_int)
  begin
    if rising_edge(clk_int) then
      if (clk_en ='1' and conv_integer(trn_cnt)/=0) then do_spi <= '0'; end if; --reset spi when done sending
      if wr_line(1)='0' and wr_line(2)='1' and a0_line(2) = '1' and addr_lock = '0' then
        buf_wr <= '1';
        tst_cnt <= tst_cnt + 1;
      else
        buf_wr <= '0';
      end if;
      
      if rd_line(1)='1' and rd_line(2)='0' and a0_line(2) = '1' and addr_lock = '0' then
        buf_rd <= '1';
      else
        buf_rd <= '0';
      end if;
      
      --Command IDs
      --pg_address 5
      --collh      1
      --coll       2
      
      if cmd_issued = '1' then --command issued
        if dti_int(7 downto 1) = "1010111"   then flags(0) <= dti_int(0); end if; --display on/off
        if dti_int(7 downto 4) = "1011"      then pg_addr <= dti_int(2 downto 0); end if;  -- assign page address
        if dti_int(7 downto 4) = "0001"      then col_addr(7 downto 4) <= dti_int(3 downto 0); addr_lock <= '0';  end if; -- assign high order column address
        if dti_int(7 downto 4) = "0000"      then col_addr(3 downto 0) <= dti_int(3 downto 0); addr_lock <= '0';  dt2spi<= "0"&pg_addr&x"2"&col_addr(7 downto 4)&dti_int(3 downto 0); do_spi <= '1'; end if; -- assign low order column address
        if dti_int(7 downto 1) = "1010000"   then flags(1) <= dti_int(0); end if; --adc select, readout order
        if dti_int(7 downto 1) = "1010011"   then flags(2) <= dti_int(0); end if; --inverse/ no inverse
        if dti_int(7 downto 1) = "1010010"   then flags(3) <= dti_int(0); end if; --whole display on/off
        if dti_int(7 downto 1) = "1010101"   then flags(4) <= dti_int(0); end if; --icon/ no icon
        if dti_int(7 downto 0) = "11100000"  then flags(5) <= '1'; col_addr_rmw <= col_addr; end if; --- read modify write, if 1 read does not autoincrement col addr on read.
        if dti_int(7 downto 0) = "11101110"  then flags(5) <= '0'; col_addr <= col_addr_rmw; addr_lock <= '0'; dt2spi<= x"0300"; do_spi <= '1'; end if; --- end read modify write
        if dti_int(7 downto 0) = "11100010"  then --reset
          flags     <= (others=>'0');
          col_addr  <= (others=>'0');
          pg_addr   <= (others=>'0');
          addr_lock <= '0'; 
        end if; 
      end if;
      
      if a0_line(2) = '1' and addr_lock = '0' and ((flags(5)='0' and rd_line(1)='1' and rd_line(2)='0') or (buf_wr = '1') ) then
        col_addr <= col_addr + 1;
        if col_addr = x"9F" then addr_lock <= '1'; end if;
      end if;
      --memory access
      if buf_rd = '1' then -- read into buffer
        dto_mem <= buf(conv_integer(pg_addr&col_addr));--get memory
      end if;
      
      if buf_wr = '1' then -- write into buffer
        buf(conv_integer(pg_addr&col_addr)) <= dti_int;
        dt2spi <= x"00"&dti_int; do_spi <= '1'; --data to be send
        --increment address
      end if;
    end if;
  end process;
  --spi process
  process(clk_int)
  begin
    if rising_edge(clk_int) then
      clk_en_1 <= clk_en;
      if clk_gate_cnt/=0 then 
        clk_gate_cnt <= clk_gate_cnt - 1;
        clk_en <='0';
      else
        clk_gate_cnt <= conv_std_logic_vector(cGATE_VAL,clk_gate_cnt'length);
        clk_en <='1';
      end if;
      
      --gated actions
      if clk_en ='1' then
        do_spi_1 <= do_spi;
        if (do_spi_1 = '0' and do_spi = '1') then
          dt2spi_i <= dt2spi;
          trn_cnt  <= conv_std_logic_vector(6, trn_cnt'length);--extra pulse to avoid freezing
          sp_cs_int <='0';
        elsif conv_integer(trn_cnt)/=0 then
            spi_done  <= '0';
            sp_cs_int <='0';
            trn_cnt <= trn_cnt - 1;
            sp_clk_int <= not sp_clk_int;
            sp_dt <= dt2spi_i(15 downto 12);
            dt2spi_i(15 downto 4)  <= dt2spi_i(11 downto 0);
            dt2spi_i(3 downto 0) <= (others=>'0');
        else
            sp_cs_int  <= '1';
            spi_done   <= '1';
            sp_clk_int <= '0';
        end if;
      end if;
    end if;
  end process;

  
  process(clk_int)
  begin
    if rising_edge(clk_int) then
      dti_int    <= dt;
      rd_line(0) <= rdn;
      wr_line(0) <= wrn;
      a0_line(0) <= a0;
      for i in 2 downto 1 loop
        rd_line(i) <= rd_line(i-1);
        wr_line(i) <= wr_line(i-1);
        a0_line(i) <= a0_line(i-1);
      end loop;
      if (rd_line(0)='0' and rd_line(1)='1') then  end if;
    end if;
  end process;

OSCInst0: osch
-- synthesis translate_off
  generic map( NOM_FREQ => "133.00" )--38MHz clock 133.00
 -- synthesis translate_on
  port map (
    STDBY          => '0',
    OSC            => clk_int,
    SEDSTDBY       => open
 );


end architecture;