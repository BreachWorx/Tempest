<?php

namespace App\Helpers;

use App\Models\ClientDevice;
use ProbablyRational\RandomNameGenerator\Alliteration;
use Illuminate\Support\Str;

class NicknameGenerator
{
    /**
     * Generate a unique PascalCase nickname (e.g. SilentPanda).
     * Appends a Discord/Reddit style discriminator (e.g. SilentPanda#1024)
     * ONLY if a collision exists in the database.
     */
    public static function generate(?int $ignoreDeviceId = null): string
    {
        // 1. Initialize the Alliteration generator
        $generator = new Alliteration();

        // Generate raw name (e.g. "silent-salmon") and convert to PascalCase ("SilentSalmon")
        $rawName = $generator->getName();
        $baseNickname = Str::studly($rawName);

        // 2. Return clean base nickname if available
        if (!static::nicknameExists($baseNickname, $ignoreDeviceId)) {
            return $baseNickname;
        }

        // 3. Collision detected: Append a Reddit/Discord style discriminator (#1000 - #9999)
        do {
            $discriminator = '#' . str_pad((string) rand(1, 9999), 4, '0', STR_PAD_LEFT);
            $nickname = $baseNickname . $discriminator;
        } while (static::nicknameExists($nickname, $ignoreDeviceId));

        return $nickname;
    }

    protected static function nicknameExists(string $nickname, ?int $ignoreDeviceId): bool
    {
        $query = ClientDevice::where('nickname', $nickname);

        if ($ignoreDeviceId) {
            $query->where('id', '!=', $ignoreDeviceId);
        }

        return $query->exists();
    }
}
