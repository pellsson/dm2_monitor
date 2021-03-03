#include <vector>
#include <string>
#include <sstream>
#include <regex>
#include <iomanip>

#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

static constexpr uint16_t hero_box_width = 40;
static constexpr uint16_t hero_box_height = 12;
static constexpr size_t num_heroes = 4;

static constexpr uint8_t DMG_LHAND = 0x01;
static constexpr uint8_t DMG_RHAND = 0x02;
static constexpr uint8_t DMG_HEAD = 0x04;
static constexpr uint8_t DMG_CHEST = 0x08;
static constexpr uint8_t DMG_LEGS = 0x10;
static constexpr uint8_t DMG_FEET = 0x20;
static constexpr uint8_t DMG_UNDEF0 = 0x40;
static constexpr uint8_t DMG_UNDEF1 = 0x80;

#define dm2_min(a, b) (((a) < (b)) ? (a) : (b))

#pragma pack(push, 1)
struct byte_status
{
	uint8_t curr;
	uint8_t full;
};

struct word_status
{
	uint16_t curr;
	uint16_t full;
};

struct hero
{
	char firstname[8];
	char lastname[0x10];

	uint8_t u0[4];
	uint8_t party_dir;
	uint8_t party_pos;

	uint8_t spell_len;
	uint8_t has_position; //??
	uint8_t lefthand_used[0x02]; // always 0??
	uint8_t spell[4];

	uint8_t u_nonzero[4];
	uint8_t left_cooldown;
	uint8_t right_cooldown;
	uint8_t spell_cooldown;
	uint8_t unknown_cooldown;
	uint16_t damage_spash_cooldown; // 0xFF 0xFF ??
	uint16_t damage_taken;
	uint8_t u_no_idea;
	uint8_t u_masked_with_3;

	uint8_t damage_bits;
	uint8_t u1;
	word_status hp;
	word_status stamina;
	word_status mp;
	uint16_t unknown_word; // FIX???
	int16_t food;
	int16_t water;
	int16_t poison; // KEK????
	// unk
	union
	{
		struct
		{
			byte_status luck;
			byte_status strength;
			byte_status dexterity;
			byte_status wisdom;
			byte_status vitality;
			byte_status anti_magic;
			byte_status anti_fire;
		};

		byte_status stats[7];
	};
	uint8_t ukek[7]; // kekekekek...
	union
	{
		struct
		{
			uint32_t fighter_exp;
			uint32_t ninja_exp;
			uint32_t priest_exp;
			uint32_t wizard_exp;
		};
		uint32_t main_exp[4];
	};
	// Warrior hidden
	uint32_t swing_exp; // swing?
	uint32_t trust_exp;
	uint32_t club_exp;
	uint32_t parry_exp;
	// Ninja hidden
	uint32_t steal_exp; // steal??
	uint32_t fight_exp;
	uint32_t throw_exp;
	uint32_t shoot_exp;
	// Priest hidden
	uint32_t identify_exp; // unverified
	uint32_t heal_exp; // unverified
	uint32_t influence_exp; // unverified
	uint32_t defend_exp; // unverified
	// Wizard hidden
	uint32_t fire_exp;
	uint32_t air_exp;
	uint32_t earth_exp;
	uint32_t water_exp;
	uint8_t inv_and_stuff[0x107-0xAF];
};

// static_assert(0x107 == sizeof(hero), "Oops");

struct hero_block
{
	hero h[num_heroes];
};
#pragma pack(pop)

struct coord
{
	int x;
	int y;
};

static constexpr coord hero_pos[num_heroes] = 
{
	{ 0, 0 },
	{ hero_box_width, 0 },
	{ hero_box_width, hero_box_height },
	{ 0, hero_box_height }
};

static int64_t find_character(const std::vector<uint8_t> &mem)
{
	static const uint8_t what[] = { "TORHAM\0\0ZED\0\0\0\0" };
	size_t n = sizeof(what);

	int64_t off = 0;

	if(n > mem.size())
	{
		return false;
	}

	const uint8_t *p = mem.data();

	while(off < (mem.size() - n))
	{
		if(*p++ == what[0])
		{
			if(0 == memcmp(p, what + 1, n - 1))
			{
				return off;
			}
		}

		++off;
	}
	return -1;
}

static std::string get_spell(const hero &h)
{
	static const char * names[] =
	{
		"Lo", "Um", "On", "Ee", "Pal", "Mon", 
		"Ya", "Vi", "Oh", "Ful", "Des", "Zo", 
		"Ven", "Ew", "Kath", "Ir", "Bro", "Gor", 
		"Ku", "Ros", "Dain", "Neta", "Ra", "Sar"
	};
	std::stringstream ss;
	for(size_t i = 0; i < static_cast<size_t>(dm2_min(h.spell_len, 4)); ++i)
	{
		if((h.spell[i] >= 0x60) && (h.spell[i] <= 0x77))
		{
			ss << names[h.spell[i] - 0x60] << " ";
		}
	}

	return ss.str();
}

//
// Linux-only stuff
//
#if defined(__GNUC__)

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>

#define sleep(n) usleep((n)*1000)

struct process
{
	int mem;
	int maps;
};

using attr_t = const char *;

static bool read_mem(const process &p, uint64_t va, void *buf, size_t n)
{
	if(va != static_cast<uint64_t>(lseek(p.mem, va, SEEK_SET)))
	{
		return false;
	}
	return n == static_cast<size_t>(read(p.mem, buf, n));
}

static uint64_t find_character_block(const process &dosbox)
{
	std::regex rx("([0-9A-Fa-f]+)-([0-9A-Fa-f]+) ([r-])([w-])([x-])(.) [0-9A-Fa-f]+ ..:.. (\\d+) +(.*)\\n");

	char c;
	std::string maps;

	while(read(dosbox.maps, &c, sizeof(c)) > 0)
	{
		maps.push_back(c);
		if('\n' == c)
		{
			std::smatch m;
			if(!std::regex_match(maps, m, rx))
			{
				printf("WARNING! Cant read maps file line...\n");
			}
			else
			{
				uint64_t start = std::strtoull(m.str(1).c_str(), NULL, 16);
				uint64_t end = std::strtoull(m.str(2).c_str(), NULL, 16);
				bool is_read = ("r" == m.str(3));
				bool is_write = ("w" == m.str(4));
				int inode = std::atoi(m.str(7).c_str());

				if(0 == inode && is_read && is_write)
				{
					std::vector<uint8_t> v;
					v.resize(end - start);
					
					if(!read_mem(dosbox, start, v.data(), v.size()))
					{
						printf("WARNING! Failed to read memory region in dosbox...\n");
						continue;
					}
					
					auto off = find_character(v);

					if(-1 != off)
					{
						return (start + off);
					}
				}

			}
			maps.clear();
		}
	}
	return 0;
}

static bool is_integer(const char *s)
{
	while(*s)
	{
		if(*s < '0' || *s > '9')
		{
			return false;
		}
		++s;
	}
	return true;
}

static bool attach_pid(pid_t pid, process &p)
{
	if(0 != ptrace(PTRACE_ATTACH, pid, nullptr, nullptr))
	{
		return false;
	}
	
	int status;
	const auto ret = waitpid(pid, &status, __WALL);
	if(ret <= 0)
	{
		return false;
	}

	std::stringstream ss;
	ss << "/proc/" << pid;
	if(-1 != (p.mem = open((ss.str() + "/mem").c_str(), O_RDONLY)))
	{
		if(-1 != (p.maps = open((ss.str() + "/maps").c_str(), O_RDONLY)))
		{
			return true;
		}
		close(p.mem);
	}
	return false;
}

static bool open_dosbox(process &p)
{
	char link_name[0x1000];
	DIR *dir = opendir("/proc/");
	dirent *ent;

	if(nullptr == dir)
	{
		return false;
	}

	while(nullptr != (ent = readdir(dir)))
	{
		if(!is_integer(ent->d_name))
		{
			continue;
		}

		std::stringstream exe;
		exe << "/proc/" << ent->d_name << "/exe";

		auto n = readlink(exe.str().c_str(), link_name, sizeof(link_name));
		if(n < 0 || n >= sizeof(link_name))
		{
			continue;
		}
		link_name[n] = 0;

		const char *base = strrchr(link_name, '/');
		if(nullptr == base)
		{
			continue;
		}
		if(0 == strcasecmp(base, "/dosbox"))
		{
			closedir(dir);
			if(attach_pid(std::atoi(ent->d_name), p))
			{
				return true;
			}
			else
			{
				printf("Unable to attach to dosbox - Are you root?\n");
				return false;
			}
		}
	}

	printf("Unable to find dosbox. Is it running?\n");
	closedir(dir);
	return true;
}

static constexpr attr_t hero_color[num_heroes] =
{
	"\x1b[92m",
	"\x1b[93m",
	"\x1b[91m",
	"\x1b[94m"
};

static void write_str(int x, int y, attr_t attr, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	printf("\x1b[%d;%dH%s", (y+1), (x+1), attr);
	vprintf(fmt, args);
	va_end(args);
}

#elif defined(WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <TlHelp32.h>

#define sleep(n) Sleep(n)

using process = HANDLE;
using attr_t = WORD;

static constexpr attr_t no_color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
static constexpr attr_t dark = FOREGROUND_INTENSITY;
static constexpr attr_t white = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
static constexpr attr_t green = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
static constexpr attr_t yellow = FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY;
static constexpr attr_t red = FOREGROUND_RED | FOREGROUND_INTENSITY;
static constexpr attr_t blue = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
static constexpr attr_t cyan = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY;

static constexpr attr_t food_good = BACKGROUND_RED | white;
static constexpr attr_t water_good = BACKGROUND_BLUE | white;
static constexpr attr_t hunger_bad = BACKGROUND_GREEN | BACKGROUND_RED | white;
static constexpr attr_t hunger_danger = BACKGROUND_RED | BACKGROUND_INTENSITY | white;
static constexpr attr_t hunger_empty = BACKGROUND_INTENSITY | white;

static constexpr attr_t hero_color[num_heroes] =
{
	green,	yellow,
	red,	blue
};

static HANDLE con0;
static HANDLE con1;

static HANDLE backbuffer;

static bool open_dosbox(process &p)
{
	HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if(INVALID_HANDLE_VALUE == h)
	{
		return false;
	}

	PROCESSENTRY32W pe32;
	pe32.dwSize = sizeof(pe32);

	if(FALSE == Process32FirstW(h, &pe32))
	{
		CloseHandle(h);
		return false;
	}

	do
	{
		if(0 == _wcsicmp(pe32.szExeFile, L"dosbox.exe"))
		{
			CloseHandle(h);

			if(nullptr == (p = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pe32.th32ProcessID)))
			{
				if(5 == GetLastError())
				{
					printf("Access denied trying to open DosBox. Please run as administrator...\n");
				}
				else
				{
					printf("Failed to open DosBox, error: %d\n", GetLastError());
				}
				return false;
			}
			return true;
		}
	}
	while(Process32NextW(h, &pe32));

	printf("DosBox not found running...\n");
	CloseHandle(h);
	return false;
}

static bool read_mem(const process &p, uint64_t va, void *buf, size_t n)
{
	SIZE_T written;
	if(!ReadProcessMemory(p, reinterpret_cast<const void *>(va), buf, n, &written)
	|| n != written)
	{
		return false;
	}
	return true;
}

static uint64_t find_character_block(HANDLE dosbox)
{
	uint8_t *p = nullptr;
	MEMORY_BASIC_INFORMATION mbi;
	while(sizeof(mbi) == VirtualQueryEx(dosbox, p, &mbi, sizeof(mbi)))
	{
		if((MEM_COMMIT == mbi.State)
		&& (MEM_PRIVATE == mbi.Type)
		&& (0 == (mbi.Protect & PAGE_GUARD)))
		{
			std::vector<uint8_t> v;
			v.resize(mbi.RegionSize);
			
			if(!read_mem(dosbox, reinterpret_cast<uint64_t>(mbi.BaseAddress), v.data(), v.size()))
			{
				printf("Warning - Can't read region %p (%d bytes)\n", mbi.BaseAddress, static_cast<uint32_t>(mbi.RegionSize));
			}
			else
			{
				auto off = find_character(v);
				if(-1 != off)
				{
					return reinterpret_cast<uint64_t>(mbi.BaseAddress) + off;
				}
			}
		}
		
		p += mbi.RegionSize;
	}

	return 0;
}

static void write_str_va(const char *fmt, va_list args)
{
	char data[128];
	vsnprintf(data, sizeof(data), fmt, args);
	data[sizeof(data) - 1] = 0;
	DWORD written;
	WriteConsoleA(backbuffer, data, strlen(data), &written, nullptr);
}

static void write_str(attr_t attr, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	SetConsoleTextAttribute(backbuffer, attr);
	write_str_va(fmt, args);
	va_end(args);
}

static void write_str(int x, int y, attr_t attr, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	SetConsoleCursorPosition(backbuffer, {static_cast<SHORT>(x), static_cast<SHORT>(y)});
	SetConsoleTextAttribute(backbuffer, attr);
	write_str_va(fmt, args);
	va_end(args);
}

static HANDLE create_con()
{
	HANDLE h = CreateConsoleScreenBuffer(GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);

	SMALL_RECT min_size = { 0, 0, 1, 1 };
	SetConsoleWindowInfo(h, TRUE, &min_size);

	if(!SetConsoleScreenBufferSize(h, { 80, 25 }))
	{
		printf("SetConsoleScreenBufferSize(): %d\n", GetLastError());
		CloseHandle(h);
		return INVALID_HANDLE_VALUE;
	}

	SMALL_RECT sr;
	sr.Top = sr.Left = 0;
	sr.Right = 80-1;
	sr.Bottom = 25-1;
	if(!SetConsoleWindowInfo(h, TRUE, &sr))
	{
		printf("SetConsoleWindowInfo(): %d\n", GetLastError());
		CloseHandle(h);
		return INVALID_HANDLE_VALUE;
	}

	return h;
}

static bool init_screen()
{
	con0 = create_con();
	con1 = create_con();

	if(INVALID_HANDLE_VALUE == con0 || INVALID_HANDLE_VALUE == con1)
	{
		return false;
	}

	SetConsoleActiveScreenBuffer(con0);
	backbuffer = con1;

	return true;
}

static void flip_screen()
{
	auto x = SetConsoleActiveScreenBuffer(backbuffer);
	backbuffer = (backbuffer == con0) ? con1 : con0;
}

static void clear_screen()
{
	CONSOLE_SCREEN_BUFFER_INFO screen;
	DWORD written;

    GetConsoleScreenBufferInfo(backbuffer, &screen);
	FillConsoleOutputCharacterA(backbuffer, ' ', screen.dwSize.X * screen.dwSize.Y, {0, 0}, &written);
	FillConsoleOutputAttribute(backbuffer, no_color,screen.dwSize.X * screen.dwSize.Y, {0, 0}, &written);
}

#else
	#error Platform not supported.
#endif

#define concat_if_true(cond, to, what) if((cond)) { to << what; }

static void write_stat_data(const word_status &s)
{
	 write_str(no_color, "% 5d", s.curr);
	 write_str(white, "/");
	 write_str(no_color, "% 5d", s.curr);
}

static void write_stat_data(const byte_status &s)
{
	 write_str(no_color, "% 3d", s.curr);
	 write_str(white, "/");
	 write_str(no_color, "% 3d", s.curr);
}

template <typename T>
static void write_stat(int x, int y, attr_t attr, char name, const T &s)
{
	write_str(x, y, attr, "%c", name);
	write_str(white, ":");
	write_stat_data(s);
}

static void write_hunger_bar(int x, int y, bool food, int16_t v)
{
	std::stringstream ss;
	// ss.precision(1);
	float perc = ((1024 + static_cast<float>(v)) / 3072.0f);
	if(perc < 0.0f)
	{
		perc = 0.0f;
	}
	else if(perc > 1.0f)
	{
		perc = 1.0f;
	}
	ss << (food ? "FOOD " : "WATR ") << std::fixed << std::setprecision(1) << (perc * 100.0f) << "% (" <<  v << ")";
	auto s = ss.str();
	while(s.length() < 19)
	{
		s.push_back(' ');
	}
	int num_full = static_cast<int>(19.0f * perc);
	attr_t fill = food ? food_good : water_good;
	if(v < 0)
	{
		fill = (v < -512) ? hunger_danger : hunger_bad;
	}

	write_str(x, y, fill, "%s", s.substr(0, num_full).c_str());
	write_str(hunger_empty, "%s", s.substr(num_full).c_str());
}

static const char *get_level_name(uint32_t exp, uint32_t &lvl)
{
	const char * const names[] =
	{
		"Neophyte",
		"Novice",
		"Apprentice",
		"Journeyman",
		"Craftsman",
		"Artisan",
		"Adept",
		"Expert",
		"Lo Master",
		"Um Master",
		"On Master",
		"Ee Master",
		"Pal Master",
		"Mon Master",
		"Archmaster",
		"Archmaster II",
		"Archmaster III",
		"Archmaster IV",
		"Archmaster V",
		"Archmaster VI",
		"Archmaster VII",
		"Archmaster VIII",
		"Archmaster IX",
	};

	int32_t level = static_cast<uint32_t>(sizeof(names) / sizeof(names[0])) - 1;
	uint32_t bit = 0x80000000;
	while(bit)
	{
		if(bit & exp)
		{
			break;
		}
		bit >>= 1;
		if(--level < 0)
		{
			lvl = 0;
			return "Unrated";
		}
	}
	lvl = static_cast<uint32_t>(level);
	return names[level];
}

static void update(const process &dosbox, uint64_t block)
{
	hero_block b;
	bool quit = false;

	if(!init_screen())
	{
		printf("No screen :(\n");
		return;
	}

	while(!quit)
	{
		if(!read_mem(dosbox, block, &b, sizeof(b)))
		{
			printf("Unable to read dosbox memory. Terminated?\n");
			break;
		}

		clear_screen();

		for(size_t i = 0; i < num_heroes; ++i)
		{ 
			const auto &h = b.h[i];
			const auto &pos = hero_pos[(h.party_pos & 0x03)];
			write_str(pos.x, pos.y, hero_color[i], "%s %s", h.firstname, h.lastname);
	
			write_stat(pos.x, pos.y + 1, green, 'H', h.hp);
			write_stat(pos.x, pos.y + 2, yellow, 'S', h.stamina);
			write_stat(pos.x, pos.y + 3, cyan, 'M', h.mp);

			
			if(0 == h.damage_bits)
			{
				write_str(pos.x + 14, pos.y + 1, dark, "No injuries.");
			}
			else
			{
				std::stringstream dmg;
				concat_if_true(h.damage_bits & DMG_LHAND, dmg, "LH ");
				concat_if_true(h.damage_bits & DMG_RHAND, dmg, "RH ");
				concat_if_true(h.damage_bits & DMG_HEAD, dmg, "HD ");
				concat_if_true(h.damage_bits & DMG_CHEST, dmg, "CS ");
				concat_if_true(h.damage_bits & DMG_LEGS, dmg, "LG ");
				concat_if_true(h.damage_bits & DMG_FEET, dmg, "FT ");
				concat_if_true(h.damage_bits & DMG_UNDEF0, dmg, "U0 ");
				concat_if_true(h.damage_bits & DMG_UNDEF1, dmg, "U1");

				write_str(pos.x + 14, pos.y + 1, red, dmg.str().c_str());
			}

			if(0 == h.poison)
			{
				write_str(pos.x + 14, pos.y + 2, dark, "Not poisoned.");
			}
			else
			{
				write_str(pos.x + 14, pos.y + 2, red, "Poison: %d", h.poison);
			}

			if(0 == h.spell_len)
			{
				write_str(pos.x + 14, pos.y + 3, dark, "Not casting.");
			}
			else
			{
				write_str(pos.x + 14, pos.y + 3, white, "%s", get_spell(h).c_str());
			}

			write_hunger_bar(pos.x, pos.y + 4, true, h.food);
			write_hunger_bar(pos.x + 20, pos.y + 4, false, h.water);

			for(int i = 0; i < 4; ++i)
			{
				static const char stats[] = "LSDW";
				write_stat(pos.x + (10 * i), pos.y + 5, white, stats[i], h.stats[i]);
			}

			for(int i = 0; i < 3; ++i)
			{
				static const char stats[] = "VMF";
				write_stat(pos.x + (10 * i), pos.y + 6, white, stats[i], h.stats[4 + i]);
			}

			for(int i = 0; i < 4; ++i)
			{
				uint32_t xp_level;
				static const char * const skills[] = { "Fighter", "Ninja", "Priest", "Wizard" };
				const char *rank = get_level_name(h.main_exp[i], xp_level);
				write_str(pos.x, pos.y + 7 + i, white, "%s %s (L:%d, X:%d)", rank, skills[i], xp_level, h.main_exp[i]);
			}
		}

		flip_screen();
		sleep(500);
	}
}

int main()
{
	process dosbox;
	printf("Searching for DosBox process...\n");

	if(!open_dosbox(dosbox))
	{
		printf("Cant open dosbox\n");
		return 1;
	}

	printf("Searching for character information block...\n");
	auto block = find_character_block(dosbox);

	if(0 == block)
	{
		printf("Unable to find character block.\nWrong main champion name? Still in menu?\n");
		return 2;
	}

	update(dosbox, block);
	return 0;
}



