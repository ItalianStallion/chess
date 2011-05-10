/*
 *  data.cpp
 */

#include "hash.hpp"
#include "defs.hpp"

int depth[2] = {3,3};

move move_to_make; // Global variable keeping track of the next move to
                   // make at the root level

int output = 1; //Used to tell the engine whether to output or not.

int chosen_evaluator = ORIGINAL; // Which evaluation function to use

int search_method = MINIMAX; // Which search method to use

int iter_depth = 5;  // See search.cpp for usage

int mpi_depth = -1;

bool bench_mode = false;

bool logging_enabled = false;

/* random numbers used to compute hash; see set_hash() in board.c */
hash_t hash_piece[2][6][64];  /* indexed by piece [color][type][square] */
hash_t hash_side;
hash_t hash_ep[64];

/* Now we have the mailbox array, so called because it looks like a
   mailbox, at least according to Bob Hyatt. This is useful when we
   need to figure out what pieces can go where. Let's say we have a
   rook on square a4 (32) and we want to know if it can move one
   square to the left. We subtract 1, and we get 31 (h5). The rook
   obviously can't move to h5, but we don't know that without doing
   a lot of annoying work. Sooooo, what we do is figure out a4's
   mailbox number, which is 61. Then we subtract 1 from 61 (60) and
   see what mailbox[60] is. In this case, it's -1, so it's out of
   bounds and we can forget it. You can see how mailbox[] is used
   in attack() in board.cpp. */

int mailbox[120] = {
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1,  0,  1,  2,  3,  4,  5,  6,  7, -1,
     -1,  8,  9, 10, 11, 12, 13, 14, 15, -1,
     -1, 16, 17, 18, 19, 20, 21, 22, 23, -1,
     -1, 24, 25, 26, 27, 28, 29, 30, 31, -1,
     -1, 32, 33, 34, 35, 36, 37, 38, 39, -1,
     -1, 40, 41, 42, 43, 44, 45, 46, 47, -1,
     -1, 48, 49, 50, 51, 52, 53, 54, 55, -1,
     -1, 56, 57, 58, 59, 60, 61, 62, 63, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

int mailbox64[64] = {
    21, 22, 23, 24, 25, 26, 27, 28,
    31, 32, 33, 34, 35, 36, 37, 38,
    41, 42, 43, 44, 45, 46, 47, 48,
    51, 52, 53, 54, 55, 56, 57, 58,
    61, 62, 63, 64, 65, 66, 67, 68,
    71, 72, 73, 74, 75, 76, 77, 78,
    81, 82, 83, 84, 85, 86, 87, 88,
    91, 92, 93, 94, 95, 96, 97, 98
};


/* slide, offsets, and offset are basically the vectors that
   pieces can move in. If slide for the piece is FALSE, it can
   only move one square in any one direction. offsets is the
   number of directions it can move in, and offset is an array
   of the actual directions. */

bool slide[6] = {
    false, false, true, true, true, false
};

int offsets[6] = {
    0, 8, 4, 4, 8, 8
};

int offset[6][8] = {
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { -21, -19, -12, -8, 8, 12, 19, 21 },
    { -11, -9, 9, 11, 0, 0, 0, 0 },
    { -10, -1, 1, 10, 0, 0, 0, 0 },
    { -11, -10, -9, -1, 1, 9, 10, 11 },
    { -11, -10, -9, -1, 1, 9, 10, 11 }
};


/* This is the castle_mask array. We can use it to determine
   the castling permissions after a move. What we do is
   logical-AND the castle bits with the castle_mask bits for
   both of the move's squares. Let's say castle is 1, meaning
   that white can still castle kingside. Now we play a move
   where the rook on h1 gets captured. We AND castle with
   castle_mask[63], so we have 1&14, and castle becomes 0 and
   white can't castle kingside anymore. */

int castle_mask[64] = {
     7, 15, 15, 15,  3, 15, 15, 11,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    13, 15, 15, 15, 12, 15, 15, 14
};


/* the piece letters, for print_board() */
char piece_char[6] = {
    'P', 'N', 'B', 'R', 'Q', 'K'
};


/* the initial board state */

int init_color[64] = {
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

int init_piece[64] = {
    3, 1, 2, 4, 5, 2, 1, 3,
    0, 0, 0, 0, 0, 0, 0, 0,
    6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6,
    0, 0, 0, 0, 0, 0, 0, 0,
    3, 1, 2, 4, 5, 2, 1, 3
};

// Most epic array of all time. Instead of generating random numbers
// which change based on the system in use,
// we will just use these. See http://xkcd.com/221/
// This allows us to have a more deterministic value to the Zobrist hash

hash_t rnum[833] = {-1375691353, -815332770, 1270435042, -815154522, -1828625225, 1123318779, 1586732881, 857454044, 1654565544, -1990485649, 2106160544, 184179035, 1209203939,
 1483071411, -2091589446, 2093958737, -773212423, -1271181000, 2015271137, -1746164393, -1500599766, 746565161, -1144048956, 1075289478, -709069829, -727667196,
  -824597445, 2008785879, 1301640596, 628405905, -1446285938, -1396859346, 1304116003, 1529756628, -1260898178, 1548840462, 1447397889, -1168954670,
   -1110898275, -1887639445, 656075682, -608339955, 814079338, 669245026, 1951647292, 729497249, -1419458719, 139036579, 906750875, -637959262, -538307760,
    -289848741, 390743293, -1392095696, -290713914, -96147628, 1829599151, -1194787978, -722043991, -1336964562, -1893933781, 711448809, 452310116, -1790640062,
     1240635447, -1090754494, -1366933250, 1026665666, 1488680893, -1182950434, -1036598065, -1518587701, -1705523470, 613658306, -462949775, 847198738,
      -732921441, -2113608628, 1158295339, -1157309526, 2062687380, 1413227593, -1024939018, -1816787874, -54007899, -1717278380, -1837297588, -1056044958, 
      -1291246157, 276592038, 1613838755, 34683531, 1350934634, 640892970, 415940656, -1949618028, 688189903, 729352085, 763586243, 1277784286, 102547099,
       -1931527498, -662674335, 1379412912, 1130758820, -1109813417, 94863152, 439861828, -656297575, -297754430, -291657712, 1935923230, 627029289, 226975126, 
       1373414176, 1056215175, 1421451740, 1251571972, -1672699341, 582724960, -610310652, 1438938119, 311337903, -1018674686, -1180634787, 1543440686, -808781742,
        512295663, -1703545903, -1887569569, -654402996, -608600128, -1165478630, -2119870803, -1272310406, 1812679606, -492895663, 609050460, -546074727,
         -1689436117, -952504417, 1078064762, 113536934, -86350164, 1456463674, -1701142525, -1226278781, 198525399, 1157930485, 1812450545, 1413032896, 1248413683,
         -1075168881, 651085383, -1520706675, 691519880, -1288911069, 1676743630, 883153765, -308042273, 1827315269, -725870439, -1642113407, 1025972529,
         60346175, -834260693, 1714914958, 1095334436, 199467091, 401005219, 2009410235, -250446121, -1386325239, -630436295, 1833561609, 1742709651, 1459353016, 1641947266, 
         1379142135, 532899165, -1362569384, 500795808, -830962876, -996676912, 1549245911, 1355179167, 271327382, -949261846, 1233054023, -1053836109, 1668182700, -1487569706,
          -544624098, 1157977139, 995174067, 2070858457, 590816556, -1575447317, 329817928, 684881059, -1828668549, -2097821451, 1081241918, -1426356220, 1904640967, 
          1587271147, -431280759, 1904249134, -574730985, -1139617870, -1178961234, -854663274, 1183979582, -315094807, 736472885, -18167186, -1546611745, 1133240316,
           17124755, -1350239844, 2046858810, -2086466194, 193299102, 2011342833, -547003777, 1807590682, -374781964, -650568704, 180864404, 685539032, -2013963463, 
           -1129280027, 1064773528, -409672602, 1604725066, 938442808, 413375282, 2127247451, -1630779532, 1948799454, -1741363010, 1487533867, -849218221, 2022189902, 
           -735174666, 1946376332, -1460818666, -188708345, 2143825943, -472547261, -128094424, 1826264571, 1569486888, 979450246, -1463464169, -311635906, -297451731, 
           817216563, 101222677, -1974054867, -1147766291, -2060887972, 1707287962, 1280643622, 974655394, 907680894, 2059208893, -2137005112, 1908886366, -1625352439, 
           345573456, -1596980567, 681984966, 1038557843, 1530313477, -1335045415, 1400194473, 1427668388, -375472568, 344559433, 949168958, 262828278, 2070051419, 
           -1210776390, 959309822, 403069609, 1715556413, 120094498, -336049045, 1950397331, -1226101646, -835948869, 1065804868, 698288285, 780812127, 406409475, 
           -559843137, 544097536, -946551559, -2011382080, 733320348, -1048186862, -353038361, -1551087253, -1290715539, 1904301307, -1340103962, -1836562869, -122773, 
           312238919, -347781465, 1587205732, -427594604, -1920337529, 1907802319, -869049189, 1582817200, 38336562, -1257585619, -1190681950, 123976489, 2095725586, 
           660883173, 1333163072, -899402007, -1745805072, -1826750344, 1673278933, 1132136481, -557967763, -173613481, -943598801, -1646576385, -1185767779, 1838379753, 
           1409329249, -764796466, 637144576, 30186182, -672433705, -554506382, 869272714, -546944310, 153236972, 1454531688, -1891628639, 1395897246, -2016360301, 
           2055404017, -55937434, 585785387, -2050326257, -1266577672, 894800972, -191550946, -1788929886, 1319552325, -1080091367, 1257936612, -410626080, -1055026548, 
           1767774339, 855670297, 873791534, -1737466439, -1920764111, 1301448760, -1979341976, 2105377795, -881268731, -939911764, -1989581621, -1640401784, -606474356, 
           -538220714, 2057392201, 297497315, -22919917, 1011396007, 1921888076, 1829473842, -1442390336, 1678104219, -737187726, -611118529, -1564841347, 889657389, 
           -197994960, 1302218290, -1607816891, -730063008, -606685813, -549842703, 690332807, 52746390, -385037492, 144835809, 1258281397, -315354791, 619132497, 
           -2083805361, -781579543, -1349976266, 1649485857, -2085490875, -56009810, -896253771, -1319967044, 122915942, -2082553209, -571714661, 786568408, 501247347, 
           -894093864, 894104255, -908197825, 1718690980, -1893016836, -243836998, 1823961932, -1999640014, 1326760877, 1938000673, -658170866, 1187124517, 1809550885, 
           -1710749122, 1751509988, 632513242, -1005512481, 726043554, -303133420, -44278973, 568601838, 1180179580, -912514772, -1889187108, 80384393, -1494454427, 
           435243275, 1507883780, 777494842, 1307519970, 779957481, -359955920, -1040276974, 165271113, 428497902, -1822406714, 64225599, 1158265374, 2038318762, 
           1317044555, 1263278645, -548542054, -994485892, -1843213881, 818075904, 1454941838, -686198328, -646751033, -510204315, -323595522, -775251282, 553360923, 
           -1083225933, -1228318013, -998645797, 2117128171, -1176446316, -1440569719, 1446398381, -409549028, 1998680878, -1587088128, -959930764, 586009656, -1570469262,
            1501714559, 2141459072, -1504268510, 1882086418, 1912361895, -516063059, -310842703, -1082497806, -660441551, -684637327, 902487239, -2024453217, 1034803275, 
            -493630353, -504666380, 1143222261, -1440524675, 1433233778, 647399391, -932062223, 319188997, 1017372678, -894409248, -2134117852, 1278832514, -12408891, 
            -1052836120, 1782461278, -988029708, 799360300, -154110670, 1088003992, -549138973, -1376752782, 753138999, -763830290, -332247794, -1943080392, -909199657, 
            516948973, -1912092042, -859308114, -565013371, 1569743918, 1628505426, -280543402, -609536342, -1721778883, 830434974, -282269279, -594066245, 992128544, 
            433927970, 2027531313, -1036055730, -2099464812, 2100125147, 1644190609, -373278536, -237645688, 1167676571, 977989817, 1199280930, -1542074841, 943834945, 
            -1353382962, 785846154, -726208052, -1652790954, 1909585367, 282077466, 60989368, -221895455, -1317228546, -329903534, 854222170, 337314794, 1305971116, 
            1137522229, -1944987349, 968914230, -483622662, 27098646, -1859312288, 23848124, 1208923116, 263442456, -2037250970, -1439193251, -959067260, 110315055, 
            -739181986, -625710345, 2049918536, -207116348, -1500082272, -1985904423, 1080874838, -617779411, -1624598356, -1035118215, 1719103191, -1276904137, 
            1266930316, 2022325921, 1594219885, -1818727277, -1894110496, -565130384, -1210887000, -149745002, -576976594, 864995759, -1602787427, 135088234, 100190937,
             993795333, 1522703810, -1911809212, 439933463, -1125501668, 1489348252, -1692425596, -1002043576, -1652911323, 1015941990, -1812171023, 457372637, 180282768, 
             -1800922358, -262363121, 2120081339, -1193545562, -1543524046, -1583174895, -2031005556, 349033442, -1785891880, -1356995852, -687177503, 780623691, 
             -1758595264, 1978409162, -1677811597, 688706128, -1194306389, 2088029583, 1901085910, -414585820, 945181588, 1732628380, 1997536572, 1921249042, 1464297509, 
             -696443016, 1974908578, 1615956839, -825773837, -904956818, 453790738, 738029432, 653664587, -437424409, -1922878181, 900617716, 1188094639, 2121486321, 
             820684703, -988742803, -241422538, 989676825, -814832498, 2038780257, 159620734, 1364360781, 1449255413, 1265871076, -717101654, 1395976052, 949042731, 
             -2137751811, 273854421, 1018624402, 1921200404, 1616784863, 211107945, 1098148302, 607585893, 975208288, -1237933390, 1269274127, 487898188, 277345838, 
             -1736718033, -2098397403, 1618402453, 2003836453, 197188199, -193847657, -2052745692, 2022634489, 1052205963, -694794693, 924738795, -134215191, -1391551474, 
             1295584409, 1718607010, 1159212596, -1263572377, 573209358, -1789252908, -1725168794, -1073713500, -5646656, 919556534, -90499447, 1554293354, -1774752365, 
             -1308259916, 1713365419, 1544721594, 1616792682, 344046183, -2013373653, -1066252094, -302573581, 1952252157, 37815513, -1745648091, 1650386572, 1723542194, 
             -722484046, 1608792354, 2136510712, -805216663, 1213934869, 1959105325, 1808643316, -1195113746, 260794778, -830703833, -770271226, 914209247, 1329702509,
              837128418, 702966889, 2146826647, -1350884408, -1119142048, -688451252, 1427902685, 1948010911, -255264230, 1041931386, -1399862577, -643520742, 2097390205, 
              72184878, 192963205, 1492848707, -497437609, 1569326999, -1157543008, -493572054, 1497219200, -1276857332, 825631433, 1323349720, -1736175242, 1353061198, 
              -83500446, -1512474165, -1920521641, 565969935, -124461125, -1402936864, -27511239, 1276363241, -1826736738, -650841617, 587174754, -1207621394, 597035948, 
              -458937340, 177321906, 2131100064, 267205535, -347682509, -1365124557, 539520824, 262437709, -358609081, -1371005820, 2030513325, -1825828174, -236169767, 
              -1950292432, 1577286567, 920893200, -908032481, 972243975, 1416637188, -801048442, 878685784, -853134546, 1417388542, 1383174484, 1238224817, -873710267, 
              56328097, -1059157426, -1966121117, -288091202, 155845611, 1062473195, -1898626185, 786599984, 744831, -782930019, 1058322004, 620275163, -1017387688, 
              -1927136883, 1953762841, -1608996307, 491743052, 1110023243, 654981824, -156656233, -1354834651, 1913577297, -1069771815, 989295738, -745285664, 706463106, 
              -273032762, 1855115570, 1369625253, -1124634729, -795264460, -1877909797, 925295611, 1230708208, 1604016295, -1590401876, 506061340, 832574895, -1996896332, 
              445392553, 523375176, -1853400155, 994301827, 236662122, -1234129967};
