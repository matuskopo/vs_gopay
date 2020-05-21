<?php

namespace GoPay;

/** Symfony container needs class for factory :( */
class Api
{
    public static function payments(array $userConfig, array $userServices = [])
    {
        $config = $userConfig + [
            'scope' => Definition\TokenScope::ALL,
            'language' => Definition\Language::ENGLISH,
            'timeout' => 30
        ];
        $services = $userServices + [
            'cache' => new Token\InMemoryTokenCache,
            'logger' => new Http\Log\NullLogger
        ];
        $browser = new Http\JsonBrowser($services['logger'], $config['timeout']);
        $gopay = new GoPay($config, $browser);
        $auth = new Token\CachedOAuth(new OAuth2($gopay), $services['cache']);
        return new Payments($gopay, $auth);
    }
}