namespace muuk {
    void init_project();

    // flag handler
    std::string normalize_flag(const std::string& flag);
    std::string normalize_flags(const std::vector<std::string>& flags);
}